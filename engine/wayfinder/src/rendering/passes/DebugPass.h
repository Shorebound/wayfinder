#pragma once

#include "rendering/backend/GPUHandles.h"
#include "rendering/backend/VertexFormats.h"
#include "rendering/graph/RenderFeature.h"

#include <vector>

namespace Wayfinder
{
    /// Debug overlay: grid, lines, unlit wire boxes (reads scene colour/depth).
    class DebugPass final : public RenderFeature
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

        RenderCapabilityMask GetCapabilities() const override
        {
            return RenderCapabilities::RASTER | RenderCapabilities::RASTER_OVERLAY_OR_DEBUG;
        }

        void OnAttach(const RenderFeatureContext& context) override;
        void OnDetach(const RenderFeatureContext& context) override;

        void AddPasses(RenderGraph& graph, const FrameRenderParams& params) override;

        /**
         * @brief Appends world-grid line vertices (same formula as the render path). Used by unit tests.
         */
        static void AppendWorldGridLineVertices(std::vector<VertexPosColour>& lineVertices, WorldGridSpec spec);

    private:
        RenderServices* m_context = nullptr;
        GPUPipelineHandle m_debugLinePipeline;
    };

} // namespace Wayfinder
