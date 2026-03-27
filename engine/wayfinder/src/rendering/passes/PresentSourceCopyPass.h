#pragma once

#include "rendering/graph/RenderPass.h"

namespace Wayfinder
{
    class RenderContext;

    /// Copies `SceneColour` into `PresentSource` so downstream passes (including composition) always sample `PresentSource`.
    class PresentSourceCopyPass final : public RenderPass
    {
    public:
        std::string_view GetName() const override
        {
            return "PresentSourceCopy";
        }

        void OnAttach(const RenderPassContext& context) override;
        void OnDetach(const RenderPassContext& context) override;
        void AddPasses(RenderGraph& graph, const RenderPipelineFrameParams& params) override;

    private:
        RenderContext* m_context = nullptr;
    };
} // namespace Wayfinder
