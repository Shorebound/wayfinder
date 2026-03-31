#pragma once

#include "rendering/backend/RenderDevice.h"
#include "rendering/backend/VertexFormats.h"

#include <array>
#include <string>
#include <unordered_map>

namespace Wayfinder
{
    class ShaderManager;

    /// Describes how to create a GPU pipeline from shader names + vertex layout + rasterizer state.
    /// The cache resolves shader bytecode through ShaderManager before creating the pipeline.
    struct GPUPipelineDesc
    {
        std::string VertexShaderName;
        std::string FragmentShaderName;
        VertexLayout VertexLayout{};
        PrimitiveType PrimitiveType = PrimitiveType::TriangleList;
        CullMode CullMode = CullMode::Back;
        FillMode FillMode = FillMode::Fill;
        FrontFace FrontFace = FrontFace::CounterClockwise;
        bool DepthTestEnabled = false;
        bool DepthWriteEnabled = false;
        uint32_t ColourTargetCount = 1;
        std::array<TextureFormat, MAX_COLOUR_TARGETS> ColourTargetFormats{};
        std::array<BlendState, MAX_COLOUR_TARGETS> ColourTargetBlends{};
    };

    /// Caches GPU pipeline handles by configuration hash.
    /// Prevents duplicate pipeline objects for identical configurations.
    /// The cache owns all pipeline handles and destroys them on Shutdown.
    class WAYFINDER_API PipelineCache
    {
    public:
        PipelineCache() = default;
        ~PipelineCache() = default;

        PipelineCache(const PipelineCache&) = delete;
        PipelineCache& operator=(const PipelineCache&) = delete;

        void Initialise(RenderDevice& device);
        void Shutdown();

        /// Destroys all cached pipelines and clears the cache.
        /// Used by ReloadShaders to force pipeline recreation.
        void InvalidateAll();

        /// Returns an existing pipeline for this configuration, or creates and caches a new one.
        /// The cache owns the returned handle — callers must not destroy it.
        GPUPipelineHandle GetOrCreate(const PipelineCreateDesc& desc);

        /// Resolves shader names through ShaderManager, converts to PipelineCreateDesc,
        /// and returns a cached or newly created pipeline handle.
        GPUPipelineHandle GetOrCreate(ShaderManager& shaders, const GPUPipelineDesc& desc);

        /// Computes a deterministic hash over all PipelineCreateDesc fields.
        static size_t HashDesc(const PipelineCreateDesc& desc);

    private:
        RenderDevice* m_device = nullptr;
        std::unordered_map<size_t, GPUPipelineHandle> m_cache;
    };

} // namespace Wayfinder
