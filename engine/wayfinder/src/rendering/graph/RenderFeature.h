#pragma once

#include "rendering/graph/RenderCapabilities.h"
#include "rendering/pipeline/FrameRenderParams.h"

#include <string_view>

namespace Wayfinder
{
    class RenderServices;
    class RenderGraph;

    /// Services made available to features during OnAttach / OnDetach.
    struct RenderFeatureContext
    {
        RenderServices& Context;
    };

    /// Base class for registerable rendering extensions (engine and game).
    /// A `RenderFeature` injects one or more passes into the per-frame render graph.
    ///
    /// Usage:
    ///   renderer.AddPass(RenderPhase::PostProcess, 0, std::make_unique<MyFeature>());
    ///
    /// Registration order follows `RenderPhase` and `order` on the pipeline; GPU execution order is
    /// still determined by resource dependencies in the graph where they apply.
    class RenderFeature
    {
    public:
        virtual ~RenderFeature() = default;

        /// Returns a unique name for this feature (used for removal and logging).
        virtual std::string_view GetName() const = 0;

        /// Bitmask of `RenderCapabilities` — scheduling/profiling/authoring; graph nodes may also call
        /// `RenderGraphBuilder::DeclarePassCapabilities` for dev-time checks in `RenderGraph::Compile`.
        virtual RenderCapabilityMask GetCapabilities() const
        {
            return RenderCapabilities::RASTER;
        }

        /// Called each frame to inject passes into the render graph.
        virtual void AddPasses(RenderGraph& graph, const FrameRenderParams& params) = 0;

        /// Called when the feature is first registered. Use the context to register
        /// shader programs, create pipelines, or acquire GPU resources.
        virtual void OnAttach(const RenderFeatureContext& /*context*/) {}

        /// Called when the feature is removed. Use the context for cleanup.
        virtual void OnDetach(const RenderFeatureContext& /*context*/) {}

        bool IsEnabled() const
        {
            return m_enabled;
        }
        virtual void SetEnabled(bool enabled)
        {
            m_enabled = enabled;
        }

    private:
        bool m_enabled = true;
    };

} // namespace Wayfinder
