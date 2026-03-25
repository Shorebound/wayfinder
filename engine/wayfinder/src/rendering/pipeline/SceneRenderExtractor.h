#pragma once

#include "rendering/graph/RenderFrame.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    class AssetService;
    class Scene;

    class WAYFINDER_API SceneRenderExtractor
    {
    public:
        RenderFrame Extract(const Scene& scene) const;
    };
} // namespace Wayfinder