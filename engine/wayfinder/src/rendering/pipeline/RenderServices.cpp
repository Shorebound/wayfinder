#include "RenderServices.h"

#include "app/EngineConfig.h"
#include "core/Log.h"
#include "rendering/backend/RenderDevice.h"

#include <SDL3/SDL.h>

#include <filesystem>

namespace Wayfinder
{
    namespace
    {
        /// Resolve a path relative to the executable base directory (same logic as ShaderManager).
        [[nodiscard]] std::string ResolvePathFromBase(std::string_view path)
        {
            const std::filesystem::path dir(path);
            if (dir.is_absolute())
            {
                return dir.string();
            }
            if (const char* base = SDL_GetBasePath())
            {
                return (std::filesystem::path(base) / dir).lexically_normal().string();
            }
            return std::string(path);
        }
    } // namespace

    Result<void> RenderServices::Initialise(RenderDevice& device, const EngineConfig& config, BlendableEffectRegistry* registry)
    {
        m_device = &device;
        m_blendableEffectRegistry = registry;

        // Initialise Slang runtime compiler if a source directory is configured
        SlangCompiler* compilerPtr = nullptr;
        if (!config.Shaders.SourceDirectory.empty())
        {
            SlangCompiler::InitDesc compilerDesc;
            const std::string resolvedSourceDir = ResolvePathFromBase(config.Shaders.SourceDirectory);
            compilerDesc.sourceDirectory = resolvedSourceDir;

            auto compilerResult = m_slangCompiler.Initialise(compilerDesc);
            if (compilerResult)
            {
                compilerPtr = &m_slangCompiler;
                WAYFINDER_INFO(LogRenderer, "RenderServices: Slang runtime compiler initialised");
            }
            else
            {
                WAYFINDER_WARN(LogRenderer, "RenderServices: Slang runtime compiler failed to initialise: {}", compilerResult.error().GetMessage());
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
        m_builtInMeshPtrs[static_cast<size_t>(BuiltInMeshId::PrimitiveColour)] = &m_primitiveMesh;
        m_builtInMeshPtrs[static_cast<size_t>(BuiltInMeshId::PrimitiveTextured)] = &m_texturedPrimitiveMesh;

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
        m_builtInMeshPtrs = {};
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
        m_slangCompiler.Shutdown();

        m_device = nullptr;
    }

    void RenderServices::ReloadShaders()
    {
        m_pipelineCache.InvalidateAll();
        m_programRegistry.InvalidateAll();
        m_shaderManager.ReloadShaders();
        WAYFINDER_INFO(LogRenderer, "RenderServices: all shaders and pipelines invalidated");
    }

} // namespace Wayfinder
