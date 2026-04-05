#pragma once

#include "ecs/Flecs.h"

namespace Wayfinder
{
    class BlendableEffectRegistry;
    struct SceneCanvas;

    /**
     * @brief Gameplay-domain extractor that reads ECS components and fills a SceneCanvas.
     *
     * This is a standalone utility class (NOT a flecs system per D-15) with cached
     * flecs queries. Called explicitly by GameplayState::OnRender().
     *
     * The Renderer never imports ECS types -- this class is the abstraction boundary
     * between gameplay and rendering (D-14). It depends only on rendering vocabulary
     * types (RenderFrame.h) and Canvas.h, never on rendering pipeline types.
     */
    class SceneRenderExtractor
    {
    public:
        /**
         * @brief Construct the extractor, caching flecs queries against the given world.
         * @param world The flecs world to query for renderable entities.
         */
        explicit SceneRenderExtractor(flecs::world& world);

        /**
         * @brief Extracts views, meshes, lights, and debug draw from ECS into the canvas.
         * @param canvas Target SceneCanvas to fill with render submissions.
         */
        void Extract(SceneCanvas& canvas) const;

        /**
         * @brief Extracts ECS data into the canvas with blendable effect resolution.
         * @param canvas Target SceneCanvas to fill with render submissions.
         * @param registry Blendable effect registry for post-process volume blending.
         */
        void Extract(SceneCanvas& canvas, const BlendableEffectRegistry* registry) const;

    private:
        flecs::world& m_world;
    };

} // namespace Wayfinder
