#pragma once

#include "core/Types.h"

namespace Wayfinder
{
    class Renderer;
}

namespace Wayfinder::Rendering
{
    /**
     * @brief Registers the engine's default feature set into a renderer.
     *
     * This populates the pipeline with SceneOpaquePass, ChromaticAberrationFeature,
     * VignetteFeature, ColourGradingFeature, DebugPass, and CompositionPass at
     * their standard phases and orders. Call before `Renderer::Initialise` so
     * features are deferred and attached in the correct lifecycle order.
     *
     * @prototype Replace with data-driven registration (TOML/JSON pipeline
     *           definition) once the asset pipeline supports it.
     */
    void RegisterDefaultFeatures(Renderer& renderer);

} // namespace Wayfinder::Rendering
