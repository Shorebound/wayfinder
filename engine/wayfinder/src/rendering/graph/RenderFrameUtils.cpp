#include "RenderFrameUtils.h"

#include "rendering/graph/RenderFrame.h"

namespace Wayfinder
{
    PreparedPrimaryView ResolvePreparedPrimaryView(const RenderFrame& frame)
    {
        PreparedPrimaryView out;
        if (frame.Views.empty() || !frame.Views.front().Prepared)
        {
            return out;
        }

        const RenderView& primary = frame.Views.front();
        out.ViewMatrix = primary.ViewMatrix;
        out.ProjectionMatrix = primary.ProjectionMatrix;
        out.ClearColour = primary.ClearColour;
        out.Valid = true;
        return out;
    }

} // namespace Wayfinder
