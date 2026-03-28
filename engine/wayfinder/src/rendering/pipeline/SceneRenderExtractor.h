#pragma once

#include "rendering/graph/RenderFrame.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    class AssetService;
    class BlendableEffectRegistry;
    class Scene;

    class WAYFINDER_API SceneRenderExtractor
    {
    public:
        /**
         * @brief Builds a `RenderFrame` from the scene (views, layers, lights, mesh submissions).
         *
         * When `registry` is non-null, blendable effect volumes are blended and applied to the
         * frame’s post-process stacks; when null, blendable effects are not resolved from volumes
         * (defaults / scene data only).
         */
        RenderFrame Extract(const Scene& scene, const BlendableEffectRegistry* registry = nullptr) const;
    };
} // namespace Wayfinder