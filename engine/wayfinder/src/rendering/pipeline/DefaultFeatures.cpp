#include "DefaultFeatures.h"

#include "Renderer.h"
#include "rendering/pipeline/RenderOrchestrator.h"

#include "rendering/passes/ChromaticAberrationFeature.h"
#include "rendering/passes/ColourGradingFeature.h"
#include "rendering/passes/CompositionPass.h"
#include "rendering/passes/DebugPass.h"
#include "rendering/passes/SceneOpaquePass.h"
#include "rendering/passes/VignetteFeature.h"

#include <memory>

namespace Wayfinder::Rendering
{
    void RegisterDefaultFeatures(Renderer& renderer)
    {
        renderer.AddFeature(RenderPhase::Opaque, 0, std::make_unique<SceneOpaquePass>());
        renderer.AddFeature(RenderPhase::PostProcess, 800, std::make_unique<ChromaticAberrationFeature>());
        renderer.AddFeature(RenderPhase::PostProcess, 900, std::make_unique<VignetteFeature>());
        renderer.AddFeature(RenderPhase::Composite, 0, std::make_unique<ColourGradingFeature>());
        renderer.AddFeature(RenderPhase::Overlay, 0, std::make_unique<DebugPass>());
        renderer.AddFeature(RenderPhase::Present, 0, std::make_unique<CompositionPass>());
    }

} // namespace Wayfinder::Rendering
