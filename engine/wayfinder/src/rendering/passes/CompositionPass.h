#pragma once

#include "rendering/graph/RenderFeature.h"

namespace Wayfinder
{
    class RenderServices;

    /// Fullscreen pass: samples via `ResolvePostProcessInput` (latest `PostProcessColour`, else `SceneColour`); applies view
    /// post-processing; writes the swapchain.
    class CompositionPass final : public RenderFeature
    {
    public:
        std::string_view GetName() const override
        {
            return "Composition";
        }

        RenderCapabilityMask GetCapabilities() const override
        {
            return RenderCapabilities::RASTER | RenderCapabilities::FULLSCREEN_COMPOSITE;
        }

        void OnAttach(const RenderFeatureContext& context) override;
        void OnDetach(const RenderFeatureContext& context) override;
        void AddPasses(RenderGraph& graph, const FrameRenderParams& params) override;

    private:
        RenderServices* m_context = nullptr;
    };
} // namespace Wayfinder
