#include "ShaderManager.h"
#include "SlangCompiler.h"
#include "core/Log.h"

#include <SDL3/SDL.h>

#include <filesystem>
#include <fstream>

namespace Wayfinder
{
    namespace
    {
        /** Relative shader paths in config are resolved against the application base path (directory containing the executable), not CWD — so IDEs can use a project working directory without breaking packaged assets
         * next to the binary. */
        [[nodiscard]] std::string ResolveShaderDirectory(std::string_view shaderDirectory)
        {
            const std::filesystem::path dir(shaderDirectory);
            if (dir.is_absolute())
            {
                return dir.string();
            }
            if (const char* base = SDL_GetBasePath())
            {
                return (std::filesystem::path(base) / dir).lexically_normal().string();
            }
            return std::string(shaderDirectory);
        }
    } // namespace

    void ShaderManager::Initialise(RenderDevice& device, std::string_view shaderDirectory, SlangCompiler* compiler)
    {
        m_device = &device;
        m_compiler = compiler;
        m_shaderDir = ResolveShaderDirectory(shaderDirectory);
        WAYFINDER_INFO(LogRenderer, "ShaderManager: initialised with directory '{}'", m_shaderDir);
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
            for (auto& [key, handle] : m_cache)
            {
                if (handle.IsValid())
                {
                    m_device->DestroyShader(handle);
                }
            }
        }
        m_cache.clear();
        WAYFINDER_INFO(LogRenderer, "ShaderManager: shader cache invalidated - shaders will recompile on next use");
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
#if !defined(WAYFINDER_SHIPPING)
        if (bytecode.empty() && m_compiler && m_compiler->IsInitialised())
        {
            const char* entryPoint = (stage == ShaderStage::Vertex) ? "VSMain" : "PSMain";
            auto compileResult = m_compiler->Compile(name, entryPoint, stage);
            if (compileResult)
            {
                bytecode = std::move(compileResult->Bytecode);
            }
            // Error already logged by SlangCompiler
        }
#endif

        if (bytecode.empty())
        {
            WAYFINDER_ERROR(LogRenderer, "ShaderManager: failed to load '{}'", filePath);
            return GPUShaderHandle::Invalid();
        }

        ShaderCreateDesc desc{};
        desc.code = bytecode.data();
        desc.codeSize = bytecode.size();
        desc.entryPoint = (stage == ShaderStage::Vertex) ? "VSMain" : "PSMain";
        desc.stage = stage;
        desc.numUniformBuffers = resources.numUniformBuffers;
        desc.numSamplers = resources.numSamplers;
        desc.numStorageTextures = resources.numStorageTextures;
        desc.numStorageBuffers = resources.numStorageBuffers;

        GPUShaderHandle handle = m_device->CreateShader(desc);
        if (!handle)
        {
            WAYFINDER_ERROR(LogRenderer, "ShaderManager: GPU shader creation failed for '{}'", filePath);
            return GPUShaderHandle::Invalid();
        }

        m_cache[key] = handle;
        WAYFINDER_INFO(LogRenderer, "ShaderManager: loaded '{}'", filePath);
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
        if (bytecode.empty())
        {
            WAYFINDER_ERROR(LogRenderer, "ShaderManager: Failed to load compute shader '{}'", filePath);
        }
        return bytecode;
    }

} // namespace Wayfinder
