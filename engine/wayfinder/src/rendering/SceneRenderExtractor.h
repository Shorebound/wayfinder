#pragma once

#include "RenderFrame.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    class Scene;

    class WAYFINDER_API SceneRenderExtractor
    {
    public:
        RenderFrame Extract(const Scene& scene) const;
    };
} // namespace Wayfinder