#include "RenderServices.h"

#include "RenderOrchestrator.h"
#include "app/EngineConfig.h"
#include "core/Log.h"
#include "platform/Paths.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/materials/SlangCompiler.h"

namespace Wayfinder
{
    RenderServices::RenderServices() = default;
    RenderServices::~RenderServices() = default;

    Result<void> RenderServices::Initialise(RenderDevice& device, const EngineConfig& config, BlendableEffectRegistry* registry)
    {
        m_device = &device;
        m_blendableEffectRegistry = registry;

        // Initialise Slang runtime compiler if a source directory is configured
        SlangCompiler* compilerPtr = nullptr;
        if (!config.Shaders.SourceDirectory.empty())
        {
            m_slangCompiler = std::make_unique<SlangCompiler>();

            SlangCompiler::InitDesc compilerDesc;
            const std::string resolvedSourceDir = Platform::ResolvePathFromBase(config.Shaders.SourceDirectory);
            compilerDesc.SourceDirectory = resolvedSourceDir;

            auto compilerResult = m_slangCompiler->Initialise(compilerDesc);
            if (compilerResult)
            {
                compilerPtr = m_slangCompiler.get();
                WAYFINDER_INFO(LogRenderer, "RenderServices: Slang runtime compiler initialised");
            }
            else
            {
                WAYFINDER_WARN(LogRenderer, "RenderServices: Slang runtime compiler failed to initialise: {}", compilerResult.error().GetMessage());
                m_slangCompiler.reset();
                // Non-fatal: fall back to pre-compiled .spv only
            }
        }

        m_shaderManager.Initialise(device, config.Shaders.Directory, compilerPtr);
        m_pipelineCache.Initialise(device);
        m_programRegistry.Initialise(device, m_shaderManager, m_pipelineCache);

        // Transient allocator: 4 MB vertex ring, 1 MB index ring.
        // May fail on Null backend (no real GPU buffers) — non-fatal in that case.
        if (!m_transientAllocator.Initialise(device, 4u * 1024u * 1024u, 1u * 1024u * 1024u))
        {
            WAYFINDER_WARN(LogRenderer, "RenderServices: Failed to initialise transient buffer allocator");
        }

        m_transientPool.Initialise(device);

        if (!m_textureManager.Initialise(device))
        {
            WAYFINDER_WARN(LogRenderer, "RenderServices: Failed to initialise TextureManager");
        }

        if (!m_meshManager.Initialise(device))
        {
            WAYFINDER_WARN(LogRenderer, "RenderServices: Failed to initialise MeshManager");
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

        // Built-in primitive meshes for non-asset draw submissions.
        m_primitiveMesh = Mesh::CreatePrimitive(device);
        m_texturedPrimitiveMesh = Mesh::CreateTexturedPrimitive(device);
        m_builtInMeshTable[static_cast<size_t>(BuiltInMeshId::PrimitiveColour)] = &m_primitiveMesh;
        m_builtInMeshTable[static_cast<size_t>(BuiltInMeshId::PrimitiveTextured)] = &m_texturedPrimitiveMesh;

        return {};
    }

    void RenderServices::SealBlendableEffects()
    {
        if (!m_blendableEffectRegistry)
        {
            WAYFINDER_WARN(LogRenderer, "SealBlendableEffects: no BlendableEffectRegistry — nothing to seal");
            return;
        }
        auto* reg = m_blendableEffectRegistry;
        if (reg->IsSealed())
        {
            return;
        }
        reg->Seal();
    }

    void RenderServices::Shutdown()
    {
        m_builtInMeshTable = {};
        m_primitiveMesh.Destroy();
        m_texturedPrimitiveMesh.Destroy();

        m_blendableEffectRegistry = nullptr;

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
        m_slangCompiler.reset();

        m_device = nullptr;
    }

    void RenderServices::ReloadShaders(RenderOrchestrator* orchestrator)
    {
        // Reset the Slang session so cached modules are discarded
        if (m_slangCompiler && m_slangCompiler->IsInitialised())
        {
            auto result = m_slangCompiler->ResetSession();
            if (!result)
            {
                WAYFINDER_ERROR(LogRenderer, "RenderServices::ReloadShaders: Slang session reset failed: {}", result.error().GetMessage());
            }
        }

        if (orchestrator)
        {
            m_pipelineCache.InvalidateAll();
            m_programRegistry.InvalidateAll();
            m_shaderManager.ReloadShaders();
            WAYFINDER_INFO(LogRenderer, "RenderServices: all shaders and pipelines invalidated");
            orchestrator->RebuildPipelines();
        }
        else
        {
            WAYFINDER_WARN(LogRenderer, "RenderServices::ReloadShaders: no orchestrator provided - deferring pipeline rebuild");
        }
    }

} // namespace Wayfinder
