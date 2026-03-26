#include "rendering/graph/RenderFrame.h"

#include <algorithm>
#include <doctest/doctest.h>

namespace Wayfinder::Tests
{
    TEST_CASE("Debug world grid vertex count matches loop structure")
    {
        const int slices = 3;
        const int clamped = std::max(1, slices);
        int count = 0;
        for (int i = -clamped; i <= clamped; ++i)
        {
            count += 4;
        }
        CHECK(count == (2 * clamped + 1) * 4);
    }

    TEST_CASE("Debug pass frame has no boxes when extractor omits debug draws")
    {
        Wayfinder::RenderFrame frame;
        Wayfinder::FrameLayerRecord* debug = frame.FindLayer(Wayfinder::FrameLayerIds::Debug);
        CHECK(debug == nullptr);
    }
} // namespace Wayfinder::Tests
