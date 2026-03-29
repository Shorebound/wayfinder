#pragma once

#include "rendering/backend/RenderDevice.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Wayfinder
{
    class SlangCompiler;

    // ── Shader Variant System ────────────────────────────────

    // Bitmask of compile-time shader features.
    // Each bit corresponds to a #define passed to the shader compiler at build time.
    // Variant 0 is the base (no defines).
    using ShaderVariantKey = uint32_t;

    enum class ShaderFeature : uint32_t
    {
        None = 0,
        VertexColour = 1 << 0,
        AlphaTest = 1 << 1,
    };

    inline ShaderVariantKey operator|(ShaderFeature a, ShaderFeature b)
    {
        return static_cast<ShaderVariantKey>(a) | static_cast<ShaderVariantKey>(b);
    }

    // Loads pre-compiled shader bytecode from disk and caches GPU shader handles.
    // Shader files are expected as .spv (SPIR-V bytecode), loaded once per name+stage+variant triple.
    class WAYFINDER_API ShaderManager
    {
    public:
        ShaderManager() = default;
        ~ShaderManager() = default;

        ShaderManager(const ShaderManager&) = delete;
        ShaderManager& operator=(const ShaderManager&) = delete;

        void Initialise(RenderDevice& device, std::string_view shaderDirectory, SlangCompiler* compiler = nullptr);
        void Shutdown();

        /**
         * @brief Invalidates all cached GPU shaders.
         *
         * Destroys every cached shader handle and clears the map.
         * The next GetShader() call will reload from .spv or recompile from .slang.
         * Callers must also invalidate pipelines that reference the old handles.
         */
        void ReloadShaders();

        // Loads bytecode from "<shaderDirectory>/<name>.vert.spv" or "<name>.frag.spv",
        // creates a GPU shader, and caches it. Returns nullptr on failure.
        // Resource counts describe the shader's bindings — passed through to ShaderCreateDesc.
        // Variant key selects a pre-compiled permutation (0 = base variant).
        GPUShaderHandle GetShader(std::string_view name, ShaderStage stage, const ShaderResourceCounts& resources = {}, ShaderVariantKey variant = 0);

        // Loads compute shader bytecode from "<shaderDirectory>/<name>.comp.spv".
        // Returns the raw bytecode for use with RenderDevice::CreateComputePipeline.
        std::vector<uint8_t> LoadComputeShaderBytecode(std::string_view name);

    private:
        struct ShaderKey
        {
            std::string name;
            ShaderStage stage;
            ShaderVariantKey variant = 0;

            bool operator==(const ShaderKey& other) const
            {
                return name == other.name && stage == other.stage && variant == other.variant;
            }
        };

        struct ShaderKeyHash
        {
            size_t operator()(const ShaderKey& k) const
            {
                size_t h1 = std::hash<std::string>{}(k.name);
                size_t h2 = std::hash<uint8_t>{}(static_cast<uint8_t>(k.stage));
                size_t h3 = std::hash<uint32_t>{}(k.variant);
                return h1 ^ (h2 << 1) ^ (h3 << 2);
            }
        };

        static std::vector<uint8_t> ReadFile(const std::string& path);

        RenderDevice* m_device = nullptr;
        SlangCompiler* m_compiler = nullptr;
        std::string m_shaderDir;
        std::unordered_map<ShaderKey, GPUShaderHandle, ShaderKeyHash> m_cache;
    };

} // namespace Wayfinder
