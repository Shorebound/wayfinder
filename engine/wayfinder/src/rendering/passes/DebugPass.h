#pragma once

#include "rendering/backend/GPUPipeline.h"
#include "rendering/graph/RenderPass.h"

namespace Wayfinder
{
    /// Debug overlay: grid, lines, unlit wire boxes (reads scene colour/depth).
    class DebugPass final : public RenderPass
    {
    public:
        std::string_view GetName() const override
        {
            return "Debug";
        }

        RenderPassCapabilityMask GetCapabilities() const override
        {
            return RenderPassCapabilities::Raster | RenderPassCapabilities::RasterOverlayOrDebug;
        }

        void OnAttach(const RenderPassContext& context) override;
        void OnDetach(const RenderPassContext& context) override;

        void AddPasses(RenderGraph& graph, const RenderPipelineFrameParams& params) override;

    private:
        RenderContext* m_context = nullptr;
        GPUPipeline m_debugLinePipeline;
    };

} // namespace Wayfinder
