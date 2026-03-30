#include "RenderFrameUtils.h"

#include "rendering/graph/RenderFrame.h"
#include "rendering/pipeline/FrameRenderParams.h"

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

    ResolvedViewForLayer ResolveViewForLayer(const FrameRenderParams& params, const size_t viewIndex)
    {
        ResolvedViewForLayer r;
        const auto& primary = params.PrimaryView;
        r.View = primary.ViewMatrix;
        r.ProjectionMatrix = primary.ProjectionMatrix;

        if (viewIndex < params.Frame.Views.size() && params.Frame.Views[viewIndex].Prepared)
        {
            const auto& pv = params.Frame.Views[viewIndex];
            r.View = pv.ViewMatrix;
            r.ProjectionMatrix = pv.ProjectionMatrix;
            r.IsValid = true;
            return r;
        }

        r.IsValid = false;
        return r;
    }

} // namespace Wayfinder::Rendering
