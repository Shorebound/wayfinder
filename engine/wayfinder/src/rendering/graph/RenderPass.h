#pragma once

#include "rendering/pipeline/RenderPipelineFrameParams.h"

#include <string_view>

namespace Wayfinder
{
    class RenderContext;
    class RenderGraph;

    /// Services made available to passes during OnAttach / OnDetach.
    struct RenderPassContext
    {
        RenderContext& Context;
    };

    /// Base class for registerable rendering extensions (engine and game).
    /// A `RenderPass` injects one or more passes into the per-frame render graph.
    ///
    /// Usage:
    ///   renderer.AddPass(std::make_unique<MyPass>());
    ///
    /// Game passes are added after engine passes (MainScene, Debug). Execution order is
    /// determined by resource dependencies in the graph, not by registration order.
    class RenderPass
    {
    public:
        virtual ~RenderPass() = default;

        /// Returns a unique name for this pass (used for removal and logging).
        virtual std::string_view GetName() const = 0;

        /// Called each frame to inject passes into the render graph.
        virtual void AddPasses(RenderGraph& graph, const RenderPipelineFrameParams& params) = 0;

        /// Called when the pass is first registered. Use the context to register
        /// shader programs, create pipelines, or acquire GPU resources.
        virtual void OnAttach(const RenderPassContext& /*context*/) {}

        /// Called when the pass is removed. Use the context for cleanup.
        virtual void OnDetach(const RenderPassContext& /*context*/) {}

        bool IsEnabled() const
        {
            return m_enabled;
        }
        void SetEnabled(bool enabled)
        {
            m_enabled = enabled;
        }

    private:
        bool m_enabled = true;
    };

} // namespace Wayfinder
