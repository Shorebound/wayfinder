#pragma once

#include "rendering/graph/RenderGraph.h"

namespace Wayfinder
{
    /// Returns PostProcessColour if any prior pass wrote it, else SceneColour.
    /// Invalid handle only if neither exists (scene pass didn't run).
    inline RenderGraphHandle ResolvePostProcessInput(const RenderGraph& graph)
    {
        const RenderGraphHandle post = graph.FindHandle(GraphTextureId::PostProcessColour);
        if (post.IsValid())
        {
            return post;
        }
        return graph.FindHandleChecked(GraphTextureId::SceneColour);
    }
} // namespace Wayfinder
