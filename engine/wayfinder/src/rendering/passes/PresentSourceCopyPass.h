#pragma once

#include "rendering/graph/RenderPass.h"

namespace Wayfinder
{
    class RenderContext;

    /// Copies `SceneColour` into `PresentSource` (fullscreen draw, identity grading) so downstream passes sample a stable
    /// handoff texture. Uses `RASTER` only — not `FULLSCREEN_COMPOSITE` (graph validates that as swapchain output).
    class PresentSourceCopyPass final : public RenderPass
    {
    public:
        std::string_view GetName() const override
        {
            return "PresentSourceCopy";
        }

        RenderPassCapabilityMask GetCapabilities() const override
        {
            return RenderPassCapabilities::RASTER;
        }

        void OnAttach(const RenderPassContext& context) override;
        void OnDetach(const RenderPassContext& context) override;
        void AddPasses(RenderGraph& graph, const RenderPipelineFrameParams& params) override;

    private:
        RenderContext* m_context = nullptr;
    };
} // namespace Wayfinder
