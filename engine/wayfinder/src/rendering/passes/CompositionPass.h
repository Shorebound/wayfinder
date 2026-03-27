#pragma once

#include "rendering/graph/RenderPass.h"

namespace Wayfinder
{
    class RenderContext;

    /// Fullscreen pass: samples `PresentSource` when registered, otherwise `SceneColour`; applies view post-processing; writes
    /// the swapchain.
    class CompositionPass final : public RenderPass
    {
    public:
        std::string_view GetName() const override
        {
            return "Composition";
        }

        RenderPassCapabilityMask GetCapabilities() const override
        {
            return RenderPassCapabilities::RASTER | RenderPassCapabilities::FULLSCREEN_COMPOSITE;
        }

        void OnAttach(const RenderPassContext& context) override;
        void OnDetach(const RenderPassContext& context) override;
        void AddPasses(RenderGraph& graph, const RenderPipelineFrameParams& params) override;

        void SetEnabled(bool enabled) override;

    private:
        RenderContext* m_context = nullptr;
    };
} // namespace Wayfinder
