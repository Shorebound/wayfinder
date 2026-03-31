#include "ShaderManager.h"
#include "SlangCompiler.h"
#include "core/Log.h"
#include "platform/Paths.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <ranges>

namespace Wayfinder
{
    void ShaderManager::Initialise(RenderDevice& device, std::string_view shaderDirectory, SlangCompiler* compiler)
    {
        m_device = &device;
        m_compiler = compiler;
        m_shaderDir = Platform::ResolvePathFromBase(shaderDirectory);

        const auto manifestPath = std::filesystem::path(m_shaderDir) / "shader_manifest.json";
        if (std::filesystem::exists(manifestPath))
        {
            LoadManifest(manifestPath.string());
        }

        Log::Info(LogRenderer, "ShaderManager: initialised with directory '{}'", m_shaderDir);
    }

    void ShaderManager::Shutdown()
    {
        for (auto& [key, handle] : m_cache)
        {
            if (handle.IsValid())
            {
                m_device->DestroyShader(handle);
            }
        }
        m_cache.clear();
        m_manifest.clear();
        m_compiler = nullptr;
        m_device = nullptr;
    }

    bool ShaderManager::LoadManifest(const std::string_view manifestPath)
    {
        const std::string pathStr(manifestPath);
        std::ifstream file(pathStr);
        if (not file.is_open())
        {
            Log::Warn(LogRenderer, "ShaderManager: could not open manifest '{}'", manifestPath);
            return false;
        }

        nlohmann::json doc;
        try
        {
            doc = nlohmann::json::parse(file);
        }
        catch (const nlohmann::json::parse_error& e)
        {
            Log::Error(LogRenderer, "ShaderManager: failed to parse manifest '{}': {}", manifestPath, e.what());
            return false;
        }

        m_manifest.clear();
        for (const auto& [shaderName, stages] : doc.items())
        {
            ManifestEntry entry{};

            auto readCounts = [](const nlohmann::json& j) -> ShaderResourceCounts
            {
                return {
                    .UniformBuffers = j.value("uniformBuffers", 0u),
                    .Samplers = j.value("samplers", 0u),
                    .StorageTextures = j.value("storageTextures", 0u),
                    .StorageBuffers = j.value("storageBuffers", 0u),
                };
            };

            if (stages.contains("vertex"))
            {
                entry.Vertex = readCounts(stages["vertex"]);
            }
            if (stages.contains("fragment"))
            {
                entry.Fragment = readCounts(stages["fragment"]);
            }

            m_manifest[shaderName] = entry;
        }

        Log::Info(LogRenderer, "ShaderManager: loaded manifest '{}' ({} shaders)", manifestPath, m_manifest.size());
        return true;
    }

    void ShaderManager::ReloadShaders()
    {
        if (m_device)
        {
            for (auto& handle : m_cache | std::views::values)
            {
                if (handle.IsValid())
                {
                    m_device->DestroyShader(handle);
                }
            }
        }
        m_cache.clear();
        Log::Info(LogRenderer, "ShaderManager: shader cache invalidated - shaders will recompile on next use");
    }

    std::optional<ShaderResourceCounts> ShaderManager::LookupManifest(const std::string_view name, const ShaderStage stage) const
    {
        const auto it = m_manifest.find(std::string(name));
        if (it == m_manifest.end())
        {
            return std::nullopt;
        }

        switch (stage)
        {
        case ShaderStage::Vertex:
            return it->second.Vertex;
        case ShaderStage::Fragment:
            return it->second.Fragment;
        default:
            return std::nullopt;
        }
    }

    GPUShaderHandle ShaderManager::GetShader(const std::string_view name, const ShaderStage stage, const ShaderVariantKey variant)
    {
        const ShaderKey key{.name = std::string(name), .stage = stage, .variant = variant};
        const auto cached = m_cache.find(key);
        if (cached != m_cache.end())
        {
            return cached->second;
        }

        const char* stageSuffix = (stage == ShaderStage::Vertex) ? ".vert.spv" : ".frag.spv";
        const std::string filePath = (std::filesystem::path(m_shaderDir) / (std::string(name) + stageSuffix)).string();

        std::vector<uint8_t> bytecode = ReadFile(filePath);
        std::optional<ShaderResourceCounts> resolvedCounts;

#if !defined(WAYFINDER_SHIPPING)
        if (bytecode.empty() and m_compiler and m_compiler->IsInitialised())
        {
            const char* entryPoint = (stage == ShaderStage::Vertex) ? "VSMain" : "PSMain";
            if (Result<SlangCompiler::CompileResult> compileResult = m_compiler->Compile(name, entryPoint, stage))
            {
                bytecode = std::move(compileResult->Bytecode);
                resolvedCounts = compileResult->Resources;
            }
            if (bytecode.empty())
            {
                Log::Error(LogRenderer, "ShaderManager: failed to load '{}' - runtime Slang compilation was attempted but produced no bytecode", filePath);
                return GPUShaderHandle::Invalid();
            }
        }
#endif

        if (bytecode.empty())
        {
            Log::Error(LogRenderer, "ShaderManager: failed to load '{}' - no pre-compiled .spv found", filePath);
            return GPUShaderHandle::Invalid();
        }

        if (not resolvedCounts)
        {
            resolvedCounts = LookupManifest(name, stage);
        }
        if (not resolvedCounts)
        {
            Log::Warn(LogRenderer, "ShaderManager: no resource counts for '{}' - shader_manifest.json entry missing; assuming zero bindings", name);
            resolvedCounts = ShaderResourceCounts{};
        }

        const ShaderResourceCounts& resources = *resolvedCounts;

        ShaderCreateDesc desc{};
        desc.Code = bytecode.data();
        desc.CodeSize = bytecode.size();
        desc.EntryPoint = (stage == ShaderStage::Vertex) ? "VSMain" : "PSMain";
        desc.Stage = stage;
        desc.UniformBuffers = resources.UniformBuffers;
        desc.Samplers = resources.Samplers;
        desc.StorageTextures = resources.StorageTextures;
        desc.StorageBuffers = resources.StorageBuffers;

        GPUShaderHandle handle = m_device->CreateShader(desc);
        if (not handle)
        {
            Log::Error(LogRenderer, "ShaderManager: GPU shader creation failed for '{}'", filePath);
            return GPUShaderHandle::Invalid();
        }

        m_cache[key] = handle;
        Log::Info(LogRenderer, "ShaderManager: loaded '{}'", filePath);
        return handle;
    }

    std::vector<uint8_t> ShaderManager::ReadFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (not file.is_open())
        {
            return {};
        }

        const std::streamsize size = file.tellg();
        if (size <= 0)
        {
            return {};
        }

        std::vector<uint8_t> buffer(static_cast<size_t>(size));
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        return buffer;
    }

    std::vector<uint8_t> ShaderManager::LoadComputeShaderBytecode(const std::string_view name)
    {
        std::string filePath = (std::filesystem::path(m_shaderDir) / (std::string(name) + ".comp.spv")).string();
        std::vector<uint8_t> bytecode = ReadFile(filePath);

#if !defined(WAYFINDER_SHIPPING)
        if (bytecode.empty() and m_compiler and m_compiler->IsInitialised())
        {
            Result<SlangCompiler::CompileResult> compileResult = m_compiler->Compile(name, "CSMain", ShaderStage::Compute);
            if (compileResult)
            {
                bytecode = std::move(compileResult->Bytecode);
            }
        }
#endif

        if (bytecode.empty())
        {
            Log::Error(LogRenderer, "ShaderManager: Failed to load compute shader '{}'", filePath);
        }
        return bytecode;
    }

} // namespace Wayfinder
