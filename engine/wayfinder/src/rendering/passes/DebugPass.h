#pragma once

#include "rendering/backend/VertexFormats.h"
#include "rendering/graph/RenderFeature.h"
#include "rendering/graph/RenderFrame.h"
#include "rendering/resources/TransientBufferAllocator.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Wayfinder
{
    /// Debug overlay: grid, lines, solid debug boxes (reads scene colour/depth).
    /// Registers two programs: `debug_unlit` (PosColour, LineList) for lines and
    /// `debug_solid` (PosNormalColour, TriangleList) for built-in box meshes.
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

        std::span<const ShaderProgramDesc> GetShaderPrograms() const override;
        void OnAttach(const RenderFeatureContext& context) override;
        void OnDetach(const RenderFeatureContext& context) override;

        void AddPasses(RenderGraph& graph, const FrameRenderParams& params) override;

        /**
         * @brief Appends world-grid line vertices (same formula as the render path). Used by unit tests.
         */
        static void AppendWorldGridLineVertices(std::vector<VertexPosColour>& lineVertices, WorldGridSpec spec);

    private:
        static constexpr uint32_t MAX_DEBUG_VIEWS = 4;

        /// Pre-computed per-view debug draw data, built during AddPasses setup
        /// and consumed by the execute lambda. Avoids per-frame hash map allocations.
        struct PerViewDebugDraw
        {
            size_t ViewIndex = 0;
            TransientAllocation LineAlloc{};
            uint32_t LineVertexCount = 0;
            uint32_t BoxStart = 0;
            uint32_t BoxCount = 0;

            /// Temporary line buffer; built per-view during setup, cleared after upload.
            std::vector<VertexPosColour> ScratchLines;

            /// Temporary box buffer; built per-view during setup, flattened into m_scratchBoxes.
            std::vector<RenderDebugBox> ScratchBoxes;
        };

        RenderServices* m_context = nullptr;

        /// Scratch buffer for box data; persists until the next frame's AddPasses.
        std::vector<RenderDebugBox> m_scratchBoxes;
    };

} // namespace Wayfinder
