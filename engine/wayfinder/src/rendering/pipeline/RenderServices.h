#pragma once

#include "FrameRenderParams.h"
#include "PipelineCache.h"
#include "core/Result.h"
#include "rendering/materials/ShaderManager.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/resources/MeshManager.h"
#include "rendering/resources/TextureManager.h"
#include "rendering/resources/TransientBufferAllocator.h"
#include "rendering/resources/TransientResourcePool.h"
#include "volumes/BlendableEffectRegistry.h"

#include <cassert>
#include <memory>

namespace Wayfinder
{
    class RenderDevice;
    class SlangCompiler;
    struct EngineConfig;

    /// Owns the shared GPU resource infrastructure used by the frame composer,
    /// render features, and anything else that needs to create or look up GPU resources.
    /// Created once at initialisation, passed by reference to subsystems.
    class WAYFINDER_API RenderServices
    {
    public:
        RenderServices();
        ~RenderServices();

        RenderServices(const RenderServices&) = delete;
        RenderServices& operator=(const RenderServices&) = delete;
        RenderServices(RenderServices&&) = delete;
        RenderServices& operator=(RenderServices&&) = delete;

        Result<void> Initialise(RenderDevice& device, const EngineConfig& config, BlendableEffectRegistry* registry = nullptr);
        void Shutdown();

        /**
         * @brief Invalidates all shaders, programs and pipelines, then rebuilds.
         *
         * Resets the Slang session (clearing cached modules), destroys all GPU
         * shader/pipeline handles, and - if an orchestrator is provided -
         * re-registers every shader program and per-pass pipeline.
         *
         * @param orchestrator  Optional. When non-null, RebuildPipelines() is
         *                      called automatically so the system is left in a
         *                      fully valid state for the next frame.
         */
        void ReloadShaders(class RenderOrchestrator* orchestrator = nullptr);

        // ── Accessors ────────────────────────────────────────
        RenderDevice& GetDevice()
        {
            assert(m_device && "RenderServices::GetDevice called before Initialise");
            return *m_device;
        }
        const RenderDevice& GetDevice() const
        {
            assert(m_device && "RenderServices::GetDevice called before Initialise");
            return *m_device;
        }

        ShaderManager& GetShaders()
        {
            return m_shaderManager;
        }
        const ShaderManager& GetShaders() const
        {
            return m_shaderManager;
        }

        PipelineCache& GetPipelines()
        {
            return m_pipelineCache;
        }
        const PipelineCache& GetPipelines() const
        {
            return m_pipelineCache;
        }

        ShaderProgramRegistry& GetPrograms()
        {
            return m_programRegistry;
        }
        const ShaderProgramRegistry& GetPrograms() const
        {
            return m_programRegistry;
        }

        TransientBufferAllocator& GetTransientBuffers()
        {
            return m_transientAllocator;
        }
        const TransientBufferAllocator& GetTransientBuffers() const
        {
            return m_transientAllocator;
        }

        TransientResourcePool& GetTransientPool()
        {
            return m_transientPool;
        }
        const TransientResourcePool& GetTransientPool() const
        {
            return m_transientPool;
        }

        TextureManager& GetTextures()
        {
            return m_textureManager;
        }
        const TextureManager& GetTextures() const
        {
            return m_textureManager;
        }

        MeshManager& GetMeshes()
        {
            return m_meshManager;
        }
        const MeshManager& GetMeshes() const
        {
            return m_meshManager;
        }

        GPUSamplerHandle GetNearestSampler() const
        {
            return m_nearestSampler;
        }

        /** @brief Registry pointer from engine initialisation; may be null in headless tests. */
        BlendableEffectRegistry* GetBlendableEffectRegistry()
        {
            return m_blendableEffectRegistry;
        }
        const BlendableEffectRegistry* GetBlendableEffectRegistry() const
        {
            return m_blendableEffectRegistry;
        }

        /** @brief Returns the built-in primitive mesh table (indexed by `BuiltInMeshId`). */
        const BuiltInMeshTable& GetBuiltInMeshes() const
        {
            assert(m_device && "RenderServices::GetBuiltInMeshes called before Initialise");
            return m_builtInMeshTable;
        }

        /// Seals the active BlendableEffectRegistry, preventing further registrations.
        /// Call after all external (game/editor) effect types have been registered.
        void SealBlendableEffects();

    private:
        RenderDevice* m_device = nullptr;
        BlendableEffectRegistry* m_blendableEffectRegistry = nullptr;

        std::unique_ptr<SlangCompiler> m_slangCompiler;
        ShaderManager m_shaderManager;
        PipelineCache m_pipelineCache;
        ShaderProgramRegistry m_programRegistry;
        TransientBufferAllocator m_transientAllocator;
        TransientResourcePool m_transientPool;
        TextureManager m_textureManager;
        MeshManager m_meshManager;
        GPUSamplerHandle m_nearestSampler{};

        Mesh m_primitiveMesh;
        Mesh m_texturedPrimitiveMesh;
        BuiltInMeshTable m_builtInMeshTable{};

        /// Set when ReloadShaders is called without an orchestrator.
        /// The next call with a valid orchestrator will complete the rebuild.
        bool m_shadersInvalidated = false;
    };

} // namespace Wayfinder
