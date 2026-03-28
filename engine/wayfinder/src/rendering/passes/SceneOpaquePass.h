#pragma once

#include "rendering/graph/RenderFeature.h"

namespace Wayfinder
{
    /// Opaque scene geometry: transient colour/depth, scene submissions via `DrawSubmission`.
    class SceneOpaquePass final : public RenderFeature
    {
    public:
        std::string_view GetName() const override
        {
            return "SceneOpaque";
        }

        RenderCapabilityMask GetCapabilities() const override
        {
            return RenderCapabilities::RASTER | RenderCapabilities::RASTER_SCENE_GEOMETRY;
        }

        void OnAttach(const RenderFeatureContext& context) override;
        void OnDetach(const RenderFeatureContext& /*context*/) override {}

        void AddPasses(RenderGraph& graph, const FrameRenderParams& params) override;

    private:
        RenderServices* m_context = nullptr;
    };

} // namespace Wayfinder
