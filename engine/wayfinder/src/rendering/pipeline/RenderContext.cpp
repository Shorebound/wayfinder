#include "RenderContext.h"

#include "app/EngineConfig.h"
#include "core/Log.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/materials/RenderingEffects.h"

namespace Wayfinder
{
    Result<void> RenderContext::Initialise(RenderDevice& device, const EngineConfig& config)
    {
        m_device = &device;

        VolumeEffectRegistry::SetActiveInstance(&m_volumeEffectRegistry);

        m_shaderManager.Initialise(device, config.Shaders.Directory);
        m_pipelineCache.Initialise(device);
        m_programRegistry.Initialise(device, m_shaderManager, m_pipelineCache);

        // Transient allocator: 4 MB vertex ring, 1 MB index ring.
        // May fail on Null backend (no real GPU buffers) — non-fatal in that case.
        if (!m_transientAllocator.Initialise(device, 4u * 1024u * 1024u, 1u * 1024u * 1024u))
        {
            WAYFINDER_WARN(LogRenderer, "RenderContext: Failed to initialise transient buffer allocator");
        }

        m_transientPool.Initialise(device);

        if (!m_textureManager.Initialise(device))
        {
            WAYFINDER_WARN(LogRenderer, "RenderContext: Failed to initialise TextureManager");
        }

        if (!m_meshManager.Initialise(device))
        {
            WAYFINDER_WARN(LogRenderer, "RenderContext: Failed to initialise MeshManager");
        }

        // Nearest-point sampler for composition blit
        {
            SamplerCreateDesc samplerDesc;
            samplerDesc.minFilter = SamplerFilter::Nearest;
            samplerDesc.magFilter = SamplerFilter::Nearest;
            samplerDesc.addressModeU = SamplerAddressMode::ClampToEdge;
            samplerDesc.addressModeV = SamplerAddressMode::ClampToEdge;
            m_nearestSampler = device.CreateSampler(samplerDesc);
        }

        return {};
    }

    void RenderContext::RegisterEngineVolumeEffects()
    {
        VolumeEffectRegistry& reg = m_volumeEffectRegistry;
        m_engineEffectIds.ColourGrading = reg.Register<ColourGradingParams>("colour_grading");
        m_engineEffectIds.Vignette = reg.Register<VignetteParams>("vignette");
        m_engineEffectIds.ChromaticAberration = reg.Register<ChromaticAberrationParams>("chromatic_aberration");
        reg.Seal();
    }

    void RenderContext::Shutdown()
    {
        VolumeEffectRegistry::SetActiveInstance(nullptr);

        if (m_nearestSampler && m_device)
        {
            m_device->DestroySampler(m_nearestSampler);
            m_nearestSampler = {};
        }

        m_transientPool.Shutdown();
        m_meshManager.Shutdown();
        m_textureManager.Shutdown();
        m_transientAllocator.Shutdown();
        m_programRegistry.Shutdown();
        m_pipelineCache.Shutdown();
        m_shaderManager.Shutdown();

        m_device = nullptr;
    }

} // namespace Wayfinder
