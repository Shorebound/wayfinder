#include "RenderContext.h"

#include "RenderDevice.h"
#include "../core/EngineConfig.h"
#include "../core/Log.h"

namespace Wayfinder
{
    bool RenderContext::Initialize(RenderDevice& device, const EngineConfig& config)
    {
        m_device = &device;

        m_shaderManager.Initialize(device, config.Shaders.Directory);
        m_pipelineCache.Initialize(device);
        m_programRegistry.Initialize(device, m_shaderManager, m_pipelineCache);

        // Transient allocator: 4 MB vertex ring, 1 MB index ring.
        // May fail on Null backend (no real GPU buffers) — non-fatal in that case.
        if (!m_transientAllocator.Initialize(device, 4u * 1024u * 1024u, 1u * 1024u * 1024u))
        {
            WAYFINDER_WARNING(LogRenderer, "RenderContext: Failed to initialize transient buffer allocator");
        }

        m_transientPool.Initialize(device);

        // Nearest-point sampler for composition blit
        {
            SamplerCreateDesc samplerDesc;
            samplerDesc.minFilter = SamplerFilter::Nearest;
            samplerDesc.magFilter = SamplerFilter::Nearest;
            samplerDesc.addressModeU = SamplerAddressMode::ClampToEdge;
            samplerDesc.addressModeV = SamplerAddressMode::ClampToEdge;
            m_nearestSampler = device.CreateSampler(samplerDesc);
        }

        return true;
    }

    void RenderContext::Shutdown()
    {
        if (m_nearestSampler && m_device)
        {
            m_device->DestroySampler(m_nearestSampler);
            m_nearestSampler = {};
        }

        m_transientPool.Shutdown();
        m_transientAllocator.Shutdown();
        m_programRegistry.Shutdown();
        m_pipelineCache.Shutdown();
        m_shaderManager.Shutdown();

        m_device = nullptr;
    }

} // namespace Wayfinder
