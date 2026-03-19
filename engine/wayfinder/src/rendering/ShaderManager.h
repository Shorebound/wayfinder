#pragma once

#include "RenderDevice.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace Wayfinder
{
    // Loads pre-compiled shader bytecode from disk and caches GPU shader handles.
    // Shader files are expected as .spv (SPIR-V bytecode), loaded once per name+stage pair.
    class WAYFINDER_API ShaderManager
    {
    public:
        ShaderManager() = default;
        ~ShaderManager() = default;

        ShaderManager(const ShaderManager&) = delete;
        ShaderManager& operator=(const ShaderManager&) = delete;

        void Initialize(RenderDevice& device, const std::string& shaderDirectory);
        void Shutdown();

        // Loads bytecode from "<shaderDirectory>/<name>.vert.spv" or "<name>.frag.spv",
        // creates a GPU shader, and caches it. Returns nullptr on failure.
        GPUShaderHandle GetShader(const std::string& name, ShaderStage stage);

        // Loads compute shader bytecode from "<shaderDirectory>/<name>.comp.spv".
        // Returns the raw bytecode for use with RenderDevice::CreateComputePipeline.
        std::vector<uint8_t> LoadComputeShaderBytecode(const std::string& name);

    private:
        struct ShaderKey
        {
            std::string name;
            ShaderStage stage;

            bool operator==(const ShaderKey& other) const
            {
                return name == other.name && stage == other.stage;
            }
        };

        struct ShaderKeyHash
        {
            size_t operator()(const ShaderKey& k) const
            {
                size_t h1 = std::hash<std::string>{}(k.name);
                size_t h2 = std::hash<uint8_t>{}(static_cast<uint8_t>(k.stage));
                return h1 ^ (h2 << 1);
            }
        };

        static std::vector<uint8_t> ReadFile(const std::string& path);

        RenderDevice* m_device = nullptr;
        std::string m_shaderDir;
        std::unordered_map<ShaderKey, GPUShaderHandle, ShaderKeyHash> m_cache;
    };

} // namespace Wayfinder
