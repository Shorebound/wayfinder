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
     * @brief Fixed ordering bands for render feature registration.
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
        PostProcess = 4, // Per-pixel and spatial effects: chromatic aberration, vignette, bloom, DOF
        Composite = 5,   // Colour-space transforms: tonemapping, colour grading
        Overlay = 6,     // Debug, editor gizmos, UI
        Present = 7,     // Pure swapchain blit (exactly one pass)
    };

    /** @brief Validates, sorts, and builds the render graph for a frame. */
    class WAYFINDER_API RenderOrchestrator
    {
    public:
        /** @brief Registers built-in shader programs and default features; stores context for BuildGraph. */
        void Initialise(RenderServices& services);
        void Shutdown();

        /**
         * @brief Registers a feature into the phase-ordered pipeline.
         *
         * If called before Initialise, the feature is deferred and flushed on Initialise.
         * @param phase Band used with @p order for stable ordering.
         * @param order Lower values run earlier within the same phase.
         * @param feature Ownership of the feature instance; must not be null.
         */
        void RegisterFeature(RenderPhase phase, int32_t order, std::unique_ptr<RenderFeature> feature);

        /**
         * @brief Re-registers all shader programs after a shader reload.
         *
         * Collects `GetShaderPrograms()` from every registered feature and re-registers them.
         * Call after ShaderManager, PipelineCache, and ShaderProgramRegistry have been invalidated.
         */
        void RebuildPipelines();

        /**
         * @brief Builds the render graph from the unified ordered pass list.
         * @param graph The render graph to populate.
         * @param params Frame parameters for pass execution.
         */
        void BuildGraph(RenderGraph& graph, const FrameRenderParams& params) const;

        /**
         * @brief Returns the first feature whose dynamic type is @p T, if any.
         * @tparam T Render feature type to match.
         * @return Pointer to the feature, or nullptr when no matching feature is registered.
         */
        template<typename T>
        const T* GetFeature() const
        {
            for (const auto& slot : m_features)
            {
                if (slot.Feature)
                {
                    if (auto* ptr = dynamic_cast<const T*>(slot.Feature.get()))
                    {
                        return ptr;
                    }
                }
            }
            for (const auto& slot : m_pendingFeatures)
            {
                if (slot.Feature)
                {
                    if (auto* ptr = dynamic_cast<const T*>(slot.Feature.get()))
                    {
                        return ptr;
                    }
                }
            }
            return nullptr;
        }

        /**
         * @brief Returns the first feature whose dynamic type is @p T, if any (non-const).
         * @tparam T Render feature type to match.
         * @return Pointer to the feature, or nullptr when no matching feature is registered.
         */
        template<typename T>
        T* GetFeature()
        {
            for (auto& slot : m_features)
            {
                if (slot.Feature)
                {
                    if (auto* ptr = dynamic_cast<T*>(slot.Feature.get()))
                    {
                        return ptr;
                    }
                }
            }
            for (auto& slot : m_pendingFeatures)
            {
                if (slot.Feature)
                {
                    if (auto* ptr = dynamic_cast<T*>(slot.Feature.get()))
                    {
                        return ptr;
                    }
                }
            }
            return nullptr;
        }

        /**
         * @brief Removes the first feature whose dynamic type is @p T from the pipeline.
         * @tparam T Render feature type to match.
         * @return True if a feature was removed; false if none matched.
         */
        template<typename T>
        bool RemoveFeature()
        {
            // Search pending features first (no OnDetach -- not yet attached).
            for (auto it = m_pendingFeatures.begin(); it != m_pendingFeatures.end(); ++it)
            {
                if (it->Feature && dynamic_cast<T*>(it->Feature.get()) != nullptr)
                {
                    m_pendingFeatures.erase(it);
                    return true;
                }
            }

            if (!m_context)
            {
                return false;
            }
            const RenderFeatureContext ctx{*m_context};
            for (auto it = m_features.begin(); it != m_features.end(); ++it)
            {
                if (it->Feature && dynamic_cast<T*>(it->Feature.get()) != nullptr)
                {
                    it->Feature->OnDetach(ctx);
                    m_features.erase(it);
                    return true;
                }
            }
            return false;
        }

    private:
        struct FeatureSlot
        {
            RenderPhase Phase = RenderPhase::Opaque;
            int32_t Order = 0;
            uint32_t InsertSequence = 0;
            std::unique_ptr<RenderFeature> Feature;
        };

        static void SortFeatureList(std::vector<FeatureSlot>& slots);

        RenderServices* m_context = nullptr;
        std::vector<FeatureSlot> m_features;
        std::vector<FeatureSlot> m_pendingFeatures;
        uint32_t m_nextInsertSequence = 0;
        bool m_initialised = false;
    };
} // namespace Wayfinder
