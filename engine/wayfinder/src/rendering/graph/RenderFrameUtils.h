#pragma once

#include "rendering/RenderTypes.h"

namespace Wayfinder
{
    struct RenderFrame;

    /// Resolved camera state for the primary view (`Views.front()` when that view is prepared).
    struct PreparedPrimaryView
    {
        Matrix4 ViewMatrix = Matrix4(1.0f);
        Matrix4 ProjectionMatrix = Matrix4(1.0f);
        Colour ClearColour = Colour::White();
        bool Valid = false;
    };

    /** @brief Primary view matrices and clear colour when `Views.front()` is prepared; otherwise `Valid` is false. */
    PreparedPrimaryView ResolvePreparedPrimaryView(const RenderFrame& frame);

} // namespace Wayfinder
