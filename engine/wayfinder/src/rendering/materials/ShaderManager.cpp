#include "ShaderManager.h"
#include "SlangCompiler.h"
#include "core/Log.h"
#include "platform/Paths.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <ranges>

namespace Wayfinder
{
    namespace
    {
        auto ShaderStageLabel(const ShaderStage stage) -> const char*
        {
            switch (stage)
            {
            case ShaderStage::Vertex:
                return "vertex";
            case ShaderStage::Fragment:
                return "fragment";
            case ShaderStage::Compute:
                return "compute";
            default:
                return "unknown";
            }
        }

        auto ReadManifestStageCounts(const nlohmann::json& stageObject, const std::string_view shaderName, const std::string_view stageName) -> ShaderResourceCounts
        {
            const auto readField = [&stageObject, shaderName, stageName](const char* fieldName) -> uint32_t
            {
                const auto field = stageObject.find(fieldName);
                if (field == stageObject.end())
                {
                    throw std::runtime_error(std::format("manifest entry '{}' stage '{}' is missing required field '{}'", shaderName, stageName, fieldName));
                }

                return field->get<uint32_t>();
            };

            return {
                .UniformBuffers = readField("uniformBuffers"),
                .Samplers = readField("samplers"),
                .StorageTextures = readField("storageTextures"),
                .StorageBuffers = readField("storageBuffers"),
            };
        }
    } // namespace

    void ShaderManager::Initialise(RenderDevice& device, std::string_view shaderDirectory, SlangCompiler* compiler)
    {
        m_device = &device;
        m_compiler = compiler;
        m_shaderDir = Platform::ResolvePathFromBase(shaderDirectory);
        m_manifestPath = (std::filesystem::path(m_shaderDir) / "shader_manifest.json").string();

        if (std::filesystem::exists(m_manifestPath))
        {
            LoadManifest(m_manifestPath);
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
        m_manifestPath.clear();
        m_compiler = nullptr;
        m_device = nullptr;
    }

    bool ShaderManager::LoadManifest(const std::string_view manifestPath)
    {
        const std::string pathStr(manifestPath);
        m_manifestPath = pathStr;
        m_manifest.clear();

        std::ifstream file(pathStr);
        if (not file.is_open())
        {
            Log::Warn(LogRenderer, "ShaderManager: could not open manifest '{}'", manifestPath);
            return false;
        }
        try
        {
            nlohmann::json doc = nlohmann::json::parse(file);
            if (not doc.is_object())
            {
                Log::Error(LogRenderer, "ShaderManager: manifest '{}' must contain a JSON object at the root", manifestPath);
                return false;
            }

            decltype(m_manifest) parsedManifest;
            for (const auto& [shaderName, stages] : doc.items())
            {
                if (not stages.is_object())
                {
                    Log::Error(LogRenderer, "ShaderManager: manifest '{}' entry '{}' must be an object keyed by stage name", manifestPath, shaderName);
                    return false;
                }

                const auto vertexIt = stages.find("vertex");
                const auto fragmentIt = stages.find("fragment");
                if (vertexIt == stages.end() or fragmentIt == stages.end())
                {
                    Log::Error(LogRenderer, "ShaderManager: manifest '{}' entry '{}' is missing required '{}' stage data", manifestPath, shaderName, (vertexIt == stages.end()) ? "vertex" : "fragment");
                    return false;
                }

                if (not vertexIt->is_object() or not fragmentIt->is_object())
                {
                    Log::Error(LogRenderer, "ShaderManager: manifest '{}' entry '{}' stage payloads must be JSON objects", manifestPath, shaderName);
                    return false;
                }

                parsedManifest.emplace(std::string(shaderName), ManifestEntry{
                                                                    .Vertex = ReadManifestStageCounts(*vertexIt, shaderName, "vertex"),
                                                                    .Fragment = ReadManifestStageCounts(*fragmentIt, shaderName, "fragment"),
                                                                });
            }

            m_manifest = std::move(parsedManifest);
        }
        catch (const nlohmann::json::exception& e)
        {
            Log::Error(LogRenderer, "ShaderManager: failed to load manifest '{}': {}", manifestPath, e.what());
            return false;
        }
        catch (const std::exception& e)
        {
            Log::Error(LogRenderer, "ShaderManager: failed to load manifest '{}': {}", manifestPath, e.what());
            return false;
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

        m_manifest.clear();
        if (not m_manifestPath.empty())
        {
            if (std::filesystem::exists(m_manifestPath))
            {
                LoadManifest(m_manifestPath);
            }
            else
            {
                Log::Info(LogRenderer, "ShaderManager: manifest '{}' not found during reload - manifest fallback disabled", m_manifestPath);
            }
        }

        Log::Info(LogRenderer, "ShaderManager: shader cache invalidated - shaders will recompile on next use");
    }

    std::optional<ShaderResourceCounts> ShaderManager::LookupManifest(const std::string_view name, const ShaderStage stage) const
    {
        const auto it = m_manifest.find(name);
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
                if (compileResult->Resources)
                {
                    resolvedCounts = *compileResult->Resources;
                }
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
            Log::Error(LogRenderer, "ShaderManager: failed to resolve resource counts for '{}' ({}) - manifest lookup failed; refusing to create shader with an unknown resource layout", name, ShaderStageLabel(stage));
            return GPUShaderHandle::Invalid();
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
