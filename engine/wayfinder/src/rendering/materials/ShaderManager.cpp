#include "ShaderManager.h"
#include "SlangCompiler.h"
#include "core/Log.h"
#include "platform/Paths.h"

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
        m_compiler = nullptr;
        m_device = nullptr;
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

    GPUShaderHandle ShaderManager::GetShader(const std::string_view name, ShaderStage stage, const ShaderResourceCounts& resources, ShaderVariantKey variant)
    {
        const ShaderKey key{.name = std::string(name), .stage = stage, .variant = variant};
        auto it = m_cache.find(key);
        if (it != m_cache.end())
        {
            return it->second;
        }

        // Build filename: <name>.vert.spv or <name>.frag.spv
        // Future: variant != 0 would append a suffix, e.g. <name>_VC.vert.spv
        const char* stageSuffix = (stage == ShaderStage::Vertex) ? ".vert.spv" : ".frag.spv";
        std::string filePath = (std::filesystem::path(m_shaderDir) / (std::string(name) + stageSuffix)).string();

        std::vector<uint8_t> bytecode = ReadFile(filePath);

        // Fallback: runtime Slang compilation when .spv is not found
        bool slangAttempted = false;
#if !defined(WAYFINDER_SHIPPING)
        if (bytecode.empty() && m_compiler && m_compiler->IsInitialised())
        {
            slangAttempted = true;
            const char* entryPoint = (stage == ShaderStage::Vertex) ? "VSMain" : "PSMain";
            if (Result<SlangCompiler::CompileResult> compileResult = m_compiler->Compile(name, entryPoint, stage))
            {
                bytecode = std::move(compileResult->Bytecode);
            }
            // Diagnostic details already logged by SlangCompiler
        }
#endif

        if (bytecode.empty())
        {
            if (slangAttempted)
            {
                Log::Error(LogRenderer, "ShaderManager: failed to load '{}' - runtime Slang compilation was attempted but produced no bytecode", filePath);
            }
            else
            {
                Log::Error(LogRenderer, "ShaderManager: failed to load '{}' - no pre-compiled .spv found", filePath);
            }
            return GPUShaderHandle::Invalid();
        }

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
        if (!handle)
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
        if (!file.is_open())
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
        if (bytecode.empty() && m_compiler && m_compiler->IsInitialised())
        {
            auto compileResult = m_compiler->Compile(name, "CSMain", ShaderStage::Compute);
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
