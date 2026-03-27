#pragma once

#include "core/Types.h"
#include "rendering/graph/RenderFrame.h"
#include "rendering/graph/RenderPass.h"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace Wayfinder
{
    class AssetService;
    class Mesh;
    class MeshManager;
    class RenderContext;
    class RenderDevice;
    class RenderGraph;
    class RenderResourceCache;

    /// Fixed ordering bands for `RenderPass` injectors. Plugins register into these phases
    /// with `order` for stable ordering within a band.
    enum class RenderPhase : uint8_t
    {
        PreOpaque = 0,   // Shadow maps, GBuffer prep, hi-Z
        Opaque = 1,      // Main scene geometry (SceneOpaquePass)
        PostOpaque = 2,  // SSAO, SSR, decals, light clustering
        Transparent = 3, // Alpha-blended geometry, particles, volumetrics
        PostProcess = 4, // Bloom, DoF, motion blur, film grain, game effects
        Composite = 5,   // FXAA/TAA, optional present-source copy
        Overlay = 6,     // Debug, editor gizmos, UI
        Present = 7,     // Tonemapping + swapchain blit (exactly one pass)
    };

    /// Validates, sorts, and builds the render graph for a frame.
    class WAYFINDER_API RenderPipeline
    {
    public:
        /// Registers built-in shader programs and stores context for BuildGraph.
        void Initialise(RenderContext& context);
        void Shutdown();

        /// Registers a pass. Requires `Initialise` to have run.
        /// Passes are ordered by `(phase, order, registration order)`.
        void RegisterPass(RenderPhase phase, int32_t order, std::unique_ptr<RenderPass> pass);

        /// Validates views/layers, pre-computes view matrices and frustums,
        /// frustum-culls submissions, then sorts by sort key.
        /// Returns false if the frame is invalid and should be skipped.
        bool Prepare(RenderFrame& frame, uint32_t swapchainWidth, uint32_t swapchainHeight) const;

        /// Builds the render graph from the unified ordered pass list.
        void BuildGraph(RenderGraph& graph, const RenderPipelineFrameParams& params) const;

        template<typename T>
        const T* GetPass() const
        {
            for (const auto& slot : m_passes)
            {
                if (slot.Pass)
                {
                    if (auto* ptr = dynamic_cast<const T*>(slot.Pass.get()))
                    {
                        return ptr;
                    }
                }
            }
            return nullptr;
        }

        template<typename T>
        T* GetPass()
        {
            for (auto& slot : m_passes)
            {
                if (slot.Pass)
                {
                    if (auto* ptr = dynamic_cast<T*>(slot.Pass.get()))
                    {
                        return ptr;
                    }
                }
            }
            return nullptr;
        }

        template<typename T>
        bool RemovePass()
        {
            if (!m_context)
            {
                return false;
            }
            const RenderPassContext ctx{*m_context};
            for (auto it = m_passes.begin(); it != m_passes.end(); ++it)
            {
                if (it->Pass && dynamic_cast<T*>(it->Pass.get()) != nullptr)
                {
                    it->Pass->OnDetach(ctx);
                    m_passes.erase(it);
                    return true;
                }
            }
            return false;
        }

    private:
        struct PassSlot
        {
            RenderPhase Phase = RenderPhase::Opaque;
            int32_t Order = 0;
            uint32_t InsertSequence = 0;
            std::unique_ptr<RenderPass> Pass;
        };

        static void SortPassList(std::vector<PassSlot>& slots);

        RenderContext* m_context = nullptr;
        std::vector<PassSlot> m_passes;
        uint32_t m_nextPassInsertSequence = 0;
        bool m_initialised = false;
    };
} // namespace Wayfinder
