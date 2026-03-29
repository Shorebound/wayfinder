#include "RenderFrameUtils.h"

#include "rendering/graph/RenderFrame.h"

namespace Wayfinder::Rendering
{
    PreparedPrimaryView ResolvePreparedPrimaryView(const RenderFrame& frame)
    {
        PreparedPrimaryView out;
        if (frame.Views.empty() || !frame.Views.front().Prepared)
        {
            return out;
        }

        const RenderView& primary = frame.Views.front();
        out.ViewIndex = 0;
        out.ViewMatrix = primary.ViewMatrix;
        out.ProjectionMatrix = primary.ProjectionMatrix;
        out.ClearColour = primary.ClearColour;
        out.Valid = true;
        return out;
    }

} // namespace Wayfinder::Rendering
