#include "RenderPipeline.h"

#include "../core/Log.h"
#include "RenderDevice.h"
#include "RenderResources.h"

namespace Wayfinder
{
    void RenderPipeline::Execute(const RenderFrame& frame, RenderDevice& device, RenderResourceCache& resources) const
    {
        if (frame.Views.empty())
        {
            WAYFINDER_WARNING(LogRenderer, "RenderPipeline: frame '{}' has no views — skipped", frame.SceneName);
            return;
        }

        if (frame.Passes.empty())
        {
            WAYFINDER_WARNING(LogRenderer, "RenderPipeline: frame '{}' has no passes — skipped", frame.SceneName);
            return;
        }

        for (const RenderPass& pass : frame.Passes)
        {
            if (!pass.Enabled || pass.Id.empty())
            {
                continue;
            }

            if (pass.ViewIndex >= frame.Views.size())
            {
                WAYFINDER_WARNING(LogRenderer, "RenderPipeline: pass '{}' references invalid view index {}", pass.Id, pass.ViewIndex);
                continue;
            }

            // Stage 4: Configure render pass descriptor per-pass (render targets, depth).
            //          Bind pipelines, vertex/index buffers, uniforms.
            //          Dispatch draw calls for each submission in this pass.
        }
    }

} // namespace Wayfinder