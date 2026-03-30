#pragma once

#include "MaterialParameter.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/backend/VertexFormats.h"
#include "wayfinder_exports.h"

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Wayfinder
{
    class Mesh;
    class PipelineCache;
    class ShaderManager;

    // ── Shader Program ───────────────────────────────────────
    // Describes a complete shader effect: which GPU shaders to use,
    // the vertex layout they expect, how many UBO slots they bind,
    // and what material parameters they declare.
    //
    // The renderer uses this to:
    //   1. Create/retrieve the GPU pipeline
    //   2. Select the correct built-in mesh (by vertex layout)
    //   3. Serialise material parameters into UBO bytes
    //   4. Push uniforms — no branching on shader name.

    struct ShaderProgramDesc
    {
        std::string Name;

        // Shader names for ShaderManager lookup
        std::string VertexShaderName;
        std::string FragmentShaderName;

        // Resource binding counts
        ShaderResourceCounts VertexResources{.UniformBuffers = 1};
        ShaderResourceCounts FragmentResources{.UniformBuffers = 1};

        // Vertex format this shader consumes
        VertexLayout VertexLayout{};

        // Rasterizer defaults
        CullMode Cull = CullMode::Back;
        bool DepthTest = true;
        bool DepthWrite = true;
        BlendState Blend{};

        // Fragment material UBO layout — declared parameter list.
        // The renderer calls SerialiseToUBO with these declarations.
        std::vector<MaterialParamDecl> MaterialParams;
        uint32_t MaterialUBOSize = 0; // Total size of the fragment material UBO in bytes

        // Vertex transform UBO size (the renderer owns the layout: MVP, Model, etc.)
        uint32_t VertexUBOSize = 0;

        // Whether this shader needs per-frame scene globals (lights, camera, time).
        // If true, the renderer pushes the SceneGlobalsUBO to fragment slot 1.
        bool NeedsSceneGlobals = false;

        // Texture slot declarations — which named texture bindings this shader expects.
        // Each slot maps to a fragment sampler binding index.
        struct TextureSlotDecl
        {
            std::string Name;     // e.g. "diffuse", "normal"
            uint32_t BindingSlot; // Fragment sampler binding index
        };
        std::vector<TextureSlotDecl> TextureSlots;
    };

    // Runtime representation of a registered shader program.
    // Created by ShaderProgramRegistry::Register().
    struct ShaderProgram
    {
        ShaderProgramDesc Desc;
        GPUPipelineHandle Pipeline{}; // Owned by PipelineCache
    };

    // ── Shader Program Registry ──────────────────────────────
    // Central registry of all available shader programs.
    // The renderer queries this by name to get pipeline + parameter layout.
    // Game developers register custom shader programs here.

    class WAYFINDER_API ShaderProgramRegistry
    {
    public:
        ShaderProgramRegistry() = default;
        ~ShaderProgramRegistry();

        ShaderProgramRegistry(const ShaderProgramRegistry&) = delete;
        ShaderProgramRegistry& operator=(const ShaderProgramRegistry&) = delete;

        // Initialise with device and shader manager references.
        void Initialise(RenderDevice& device, ShaderManager& shaders, PipelineCache& cache);
        void Shutdown();

        // Register a shader program. Creates the GPU pipeline immediately.
        // Returns true on success.
        bool Register(const ShaderProgramDesc& desc);

        // Look up a registered program by name. Returns nullptr if not found.
        const ShaderProgram* Find(std::string_view name) const;

        // Convenience: find with fallback to a default program.
        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — primary name vs fallback shader name
        const ShaderProgram* FindOrDefault(std::string_view name, std::string_view fallback = "unlit") const;

        /**
         * @brief Clears all registered programs.
         *
         * Used by ReloadShaders so programs can be re-registered with freshly compiled shaders.
         */
        void InvalidateAll();

        /**
         * @brief Returns a pipeline variant for the named program with a different primitive topology.
         *
         * Caches the result so repeated calls with the same (name, topology) are free.
         *
         * @param name     Name of the registered shader program.
         * @param topology Primitive topology for the variant pipeline.
         * @return A valid GPUPipelineHandle on success, or an invalid handle if the program
         *         is not found or pipeline creation fails.
         */
        GPUPipelineHandle GetVariantPipeline(std::string_view name, PrimitiveType topology);

    private:
        RenderDevice* m_device = nullptr;
        ShaderManager* m_shaders = nullptr;
        PipelineCache* m_cache = nullptr;

        std::unordered_map<std::string, ShaderProgram, TransparentStringHash, std::equal_to<>> m_programs;

        /// Key for variant pipelines: (program name, PrimitiveType).
        struct VariantKey
        {
            std::string Name;
            PrimitiveType Topology = PrimitiveType::TriangleList;

            bool operator==(const VariantKey&) const = default;
        };

        struct VariantKeyHash
        {
            size_t operator()(const VariantKey& k) const
            {
                size_t h = std::hash<std::string>{}(k.Name);
                h ^= std::hash<int>{}(static_cast<int>(k.Topology)) + 0x9e3779b9 + (h << 6) + (h >> 2);
                return h;
            }
        };

        /// Variant pipelines keyed by (program name, PrimitiveType).
        std::unordered_map<VariantKey, GPUPipelineHandle, VariantKeyHash> m_variantPipelines;
    };

} // namespace Wayfinder
