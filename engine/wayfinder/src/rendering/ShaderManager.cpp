#include "ShaderManager.h"
#include "../core/Log.h"

#include <fstream>
#include <filesystem>

namespace Wayfinder
{
    void ShaderManager::Initialize(RenderDevice& device, const std::string& shaderDirectory)
    {
        m_device = &device;
        m_shaderDir = shaderDirectory;
        WAYFINDER_INFO(LogRenderer, "ShaderManager: initialized with directory '{}'", m_shaderDir);
    }

    void ShaderManager::Shutdown()
    {
        for (auto& [key, handle] : m_cache)
        {
            if (handle)
            {
                m_device->DestroyShader(handle);
            }
        }
        m_cache.clear();
        m_device = nullptr;
    }

    GPUShaderHandle ShaderManager::GetShader(const std::string& name, ShaderStage stage,
                                              const ShaderResourceCounts& resources,
                                              ShaderVariantKey variant)
    {
        ShaderKey key{name, stage, variant};
        auto it = m_cache.find(key);
        if (it != m_cache.end())
        {
            return it->second;
        }

        // Build filename: <name>.vert.spv or <name>.frag.spv
        // Future: variant != 0 would append a suffix, e.g. <name>_VC.vert.spv
        const char* stageSuffix = (stage == ShaderStage::Vertex) ? ".vert.spv" : ".frag.spv";
        std::string filePath = (std::filesystem::path(m_shaderDir) / (name + stageSuffix)).string();

        std::vector<uint8_t> bytecode = ReadFile(filePath);
        if (bytecode.empty())
        {
            WAYFINDER_ERROR(LogRenderer, "ShaderManager: Failed to load '{}'", filePath);
            return nullptr;
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
            return nullptr;
        }

        m_cache[key] = handle;
        WAYFINDER_INFO(LogRenderer, "ShaderManager: Loaded '{}'", filePath);
        return handle;
    }

    std::vector<uint8_t> ShaderManager::ReadFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            return {};
        }

        std::streamsize size = file.tellg();
        if (size <= 0)
        {
            return {};
        }

        std::vector<uint8_t> buffer(static_cast<size_t>(size));
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        return buffer;
    }

    std::vector<uint8_t> ShaderManager::LoadComputeShaderBytecode(const std::string& name)
    {
        std::string filePath = (std::filesystem::path(m_shaderDir) / (name + ".comp.spv")).string();
        std::vector<uint8_t> bytecode = ReadFile(filePath);
        if (bytecode.empty())
        {
            WAYFINDER_ERROR(LogRenderer, "ShaderManager: Failed to load compute shader '{}'", filePath);
        }
        return bytecode;
    }

} // namespace Wayfinder
