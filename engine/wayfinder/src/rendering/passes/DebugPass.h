#pragma once

#include "rendering/backend/GPUPipeline.h"
#include "rendering/backend/VertexFormats.h"
#include "rendering/graph/RenderPass.h"

#include <vector>

namespace Wayfinder
{
    /// Debug overlay: grid, lines, unlit wire boxes (reads scene colour/depth).
    class DebugPass final : public RenderPass
    {
    public:
        /** Parameters for world-grid line generation (shared with unit tests). */
        struct WorldGridSpec
        {
            int Slices;
            float Spacing;
        };

        std::string_view GetName() const override
        {
            return "Debug";
        }

        RenderPassCapabilityMask GetCapabilities() const override
        {
            return RenderPassCapabilities::RASTER | RenderPassCapabilities::RASTER_OVERLAY_OR_DEBUG;
        }

        void OnAttach(const RenderPassContext& context) override;
        void OnDetach(const RenderPassContext& context) override;

        void AddPasses(RenderGraph& graph, const RenderPipelineFrameParams& params) override;

        /**
         * @brief Appends world-grid line vertices (same formula as the render path). Used by unit tests.
         */
        static void AppendWorldGridLineVertices(std::vector<VertexPosColour>& lineVertices, WorldGridSpec spec);

    private:
        RenderContext* m_context = nullptr;
        GPUPipeline m_debugLinePipeline;
    };

} // namespace Wayfinder
