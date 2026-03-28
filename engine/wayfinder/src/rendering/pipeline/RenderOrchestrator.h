#pragma once

#include "core/Types.h"
#include "rendering/graph/RenderFeature.h"
#include "rendering/graph/RenderFrame.h"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace Wayfinder
{
    class AssetService;
    class Mesh;
    class MeshManager;
    class RenderServices;
    class RenderDevice;
    class RenderGraph;
    class RenderResourceCache;

    /**
     * @brief Fixed ordering bands for render pass registration.
     *
     * Plugins register into a phase with an `order` value for stable ordering within the band.
     * Phases are executed in ascending numeric order.
     */
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

    /** @brief Validates, sorts, and builds the render graph for a frame. */
    class WAYFINDER_API RenderOrchestrator
    {
    public:
        /** @brief Registers built-in shader programs and default passes; stores context for BuildGraph. */
        void Initialise(RenderServices& services);
        void Shutdown();

        /**
         * @brief Registers a pass into the phase-ordered pipeline.
         *
         * If called before Initialise, the pass is deferred and flushed on Initialise.
         * @param phase Band used with @p order for stable ordering.
         * @param order Lower values run earlier within the same phase.
         * @param pass Ownership of the pass instance; must not be null.
         */
        void RegisterPass(RenderPhase phase, int32_t order, std::unique_ptr<RenderFeature> pass);

        /**
         * @brief Validates views/layers, pre-computes view matrices and frustums,
         *        frustum-culls submissions, then sorts by sort key.
         * @param frame The render frame to prepare (modified in place).
         * @param swapchainWidth Current swapchain width in pixels.
         * @param swapchainHeight Current swapchain height in pixels.
         * @return False if the frame is invalid and should be skipped.
         */
        bool Prepare(RenderFrame& frame, uint32_t swapchainWidth, uint32_t swapchainHeight) const;

        /**
         * @brief Builds the render graph from the unified ordered pass list.
         * @param graph The render graph to populate.
         * @param params Frame parameters for pass execution.
         */
        void BuildGraph(RenderGraph& graph, const FrameRenderParams& params) const;

        /**
         * @brief Returns the first pass whose dynamic type is @p T, if any.
         * @tparam T Render pass type to match.
         * @return Pointer to the pass, or nullptr when no matching pass is registered.
         */
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
            for (const auto& slot : m_pendingPasses)
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

        /**
         * @brief Returns the first pass whose dynamic type is @p T, if any (non-const).
         * @tparam T Render pass type to match.
         * @return Pointer to the pass, or nullptr when no matching pass is registered.
         */
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
            for (auto& slot : m_pendingPasses)
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

        /**
         * @brief Removes the first pass whose dynamic type is @p T from the pipeline.
         * @tparam T Render pass type to match.
         * @return True if a pass was removed; false if none matched.
         */
        template<typename T>
        bool RemovePass()
        {
            // Search pending passes first (no OnDetach — not yet attached).
            for (auto it = m_pendingPasses.begin(); it != m_pendingPasses.end(); ++it)
            {
                if (it->Pass && dynamic_cast<T*>(it->Pass.get()) != nullptr)
                {
                    m_pendingPasses.erase(it);
                    return true;
                }
            }

            if (!m_context)
            {
                return false;
            }
            const RenderFeatureContext ctx{*m_context};
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
            std::unique_ptr<RenderFeature> Pass;
        };

        static void SortPassList(std::vector<PassSlot>& slots);

        RenderServices* m_context = nullptr;
        std::vector<PassSlot> m_passes;
        std::vector<PassSlot> m_pendingPasses;
        uint32_t m_nextPassInsertSequence = 0;
        bool m_initialised = false;
    };
} // namespace Wayfinder
