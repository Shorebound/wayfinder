#include "rendering/backend/VertexFormats.h"
#include "rendering/graph/RenderFrame.h"
#include "rendering/passes/DebugPass.h"

#include <algorithm>
#include <vector>

#include <doctest/doctest.h>

namespace Wayfinder::Tests
{
    TEST_CASE("Debug world grid vertex count matches grid builder")
    {
        std::vector<VertexPosColour> vertices;
        const int slices = 3;
        Wayfinder::DebugPass::AppendWorldGridLineVertices(vertices, {.Slices = slices, .Spacing = 1.0f});

        const int clamped = std::max(1, slices);
        CHECK(vertices.size() == static_cast<size_t>((2 * clamped + 1) * 4));
    }

    TEST_CASE("Debug layer is absent when frame has no debug layer (extractor omits debug draws)")
    {
        Wayfinder::RenderFrame frame;
        Wayfinder::FrameLayer* debug = frame.FindLayer(Wayfinder::FrameLayerIds::Debug);
        CHECK(debug == nullptr);
    }
} // namespace Wayfinder::Tests
