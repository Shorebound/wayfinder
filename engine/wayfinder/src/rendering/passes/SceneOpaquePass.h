#pragma once

#include "rendering/graph/RenderPass.h"

namespace Wayfinder
{
    /// Opaque scene geometry: transient colour/depth, scene submissions via `DrawSubmission`.
    class SceneOpaquePass final : public RenderPass
    {
    public:
        std::string_view GetName() const override
        {
            return "SceneOpaque";
        }

        RenderPassCapabilityMask GetCapabilities() const override
        {
            return RenderPassCapabilities::Raster | RenderPassCapabilities::RasterSceneGeometry;
        }

        void OnAttach(const RenderPassContext& context) override;
        void OnDetach(const RenderPassContext& /*context*/) override {}

        void AddPasses(RenderGraph& graph, const RenderPipelineFrameParams& params) override;

    private:
        RenderContext* m_context = nullptr;
    };

} // namespace Wayfinder
