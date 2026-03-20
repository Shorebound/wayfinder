#include "RenderPipeline.h"

#include "../core/Log.h"
#include "RenderDevice.h"
#include "RenderResources.h"

#include <algorithm>

namespace Wayfinder
{
    bool RenderPipeline::Prepare(RenderFrame& frame) const
    {
        if (frame.Views.empty())
        {
            WAYFINDER_WARNING(LogRenderer, "RenderPipeline: frame '{}' has no views — skipped", frame.SceneName);
            return false;
        }

        if (frame.Passes.empty())
        {
            WAYFINDER_WARNING(LogRenderer, "RenderPipeline: frame '{}' has no passes — skipped", frame.SceneName);
            return false;
        }

        for (RenderPass& pass : frame.Passes)
        {
            if (!pass.Enabled || pass.Id.IsEmpty())
            {
                continue;
            }

            if (pass.ViewIndex >= frame.Views.size())
            {
                WAYFINDER_WARNING(LogRenderer, "RenderPipeline: pass '{}' references invalid view index {}", pass.Id, pass.ViewIndex);
                pass.Enabled = false;
                continue;
            }

            // Sort scene pass submissions by sort key (front-to-back for opaque)
            if (pass.Kind == RenderPassKind::Scene)
            {
                std::sort(pass.Meshes.begin(), pass.Meshes.end(),
                    [](const RenderMeshSubmission& a, const RenderMeshSubmission& b)
                    {
                        return a.SortKey < b.SortKey;
                    });
            }
        }

        return true;
    }

} // namespace Wayfinder