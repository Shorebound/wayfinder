#pragma once

#include "PipelineCache.h"
#include "rendering/materials/ShaderManager.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/resources/TextureManager.h"
#include "rendering/resources/TransientBufferAllocator.h"
#include "rendering/resources/TransientResourcePool.h"

#include <cassert>

namespace Wayfinder
{
    class RenderDevice;
    struct EngineConfig;

    /// Owns the shared GPU resource infrastructure used by the render pipeline,
    /// features, and anything else that needs to create or look up GPU resources.
    /// Created once at initialisation, passed by reference to subsystems.
    class WAYFINDER_API RenderContext
    {
    public:
        RenderContext() = default;
        ~RenderContext() = default;

        RenderContext(const RenderContext&) = delete;
        RenderContext& operator=(const RenderContext&) = delete;
        RenderContext(RenderContext&&) = delete;
        RenderContext& operator=(RenderContext&&) = delete;

        bool Initialise(RenderDevice& device, const EngineConfig& config);
        void Shutdown();

        // ── Accessors ────────────────────────────────────────
        RenderDevice& GetDevice() { assert(m_device && "RenderContext::GetDevice called before Initialise"); return *m_device; }
        const RenderDevice& GetDevice() const { assert(m_device && "RenderContext::GetDevice called before Initialise"); return *m_device; }

        ShaderManager& GetShaders() { return m_shaderManager; }
        const ShaderManager& GetShaders() const { return m_shaderManager; }

        PipelineCache& GetPipelines() { return m_pipelineCache; }
        const PipelineCache& GetPipelines() const { return m_pipelineCache; }

        ShaderProgramRegistry& GetPrograms() { return m_programRegistry; }
        const ShaderProgramRegistry& GetPrograms() const { return m_programRegistry; }

        TransientBufferAllocator& GetTransientBuffers() { return m_transientAllocator; }
        const TransientBufferAllocator& GetTransientBuffers() const { return m_transientAllocator; }

        TransientResourcePool& GetTransientPool() { return m_transientPool; }
        const TransientResourcePool& GetTransientPool() const { return m_transientPool; }

        TextureManager& GetTextures() { return m_textureManager; }
        const TextureManager& GetTextures() const { return m_textureManager; }

        GPUSamplerHandle GetNearestSampler() const { return m_nearestSampler; }

    private:
        RenderDevice* m_device = nullptr;

        ShaderManager m_shaderManager;
        PipelineCache m_pipelineCache;
        ShaderProgramRegistry m_programRegistry;
        TransientBufferAllocator m_transientAllocator;
        TransientResourcePool m_transientPool;
        TextureManager m_textureManager;
        GPUSamplerHandle m_nearestSampler{};
    };

} // namespace Wayfinder
