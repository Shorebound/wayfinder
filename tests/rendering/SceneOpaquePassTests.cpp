#include "app/EngineConfig.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/graph/RenderFrame.h"
#include "rendering/graph/RenderFrameUtils.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/passes/ChromaticAberrationFeature.h"
#include "rendering/passes/ColourGradingFeature.h"
#include "rendering/passes/CompositionPass.h"
#include "rendering/passes/DebugPass.h"
#include "rendering/passes/SceneOpaquePass.h"
#include "rendering/passes/VignetteFeature.h"
#include "rendering/pipeline/PrepareFrame.h"
#include "rendering/pipeline/RenderOrchestrator.h"
#include "rendering/pipeline/RenderServices.h"

#include <doctest/doctest.h>

namespace Wayfinder::Tests
{
    TEST_CASE("SceneOpaquePass: BuildGraph compiles with default pipeline")
    {
        auto device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null);
        REQUIRE(device);

        Wayfinder::EngineConfig config;
        config.Window.Width = 320;
        config.Window.Height = 240;

        Wayfinder::RenderServices context;
        REQUIRE(context.Initialise(*device, config));

        Wayfinder::RenderOrchestrator pipeline;
        pipeline.Initialise(context);

        pipeline.RegisterFeature(Wayfinder::RenderPhase::Opaque, 0, std::make_unique<Wayfinder::SceneOpaquePass>());
        pipeline.RegisterFeature(Wayfinder::RenderPhase::PostProcess, 800, std::make_unique<Wayfinder::ChromaticAberrationFeature>());
        pipeline.RegisterFeature(Wayfinder::RenderPhase::PostProcess, 900, std::make_unique<Wayfinder::Rendering::VignetteFeature>());
        pipeline.RegisterFeature(Wayfinder::RenderPhase::Composite, 0, std::make_unique<Wayfinder::ColourGradingFeature>());
        pipeline.RegisterFeature(Wayfinder::RenderPhase::Overlay, 0, std::make_unique<Wayfinder::DebugPass>());
        pipeline.RegisterFeature(Wayfinder::RenderPhase::Present, 0, std::make_unique<Wayfinder::CompositionPass>());

        Wayfinder::RenderFrame frame;
        Wayfinder::RenderView view;
        view.CameraState.Position = {0.0f, 0.0f, 5.0f};
        view.CameraState.Target = {0.0f, 0.0f, 0.0f};
        view.CameraState.Up = {0.0f, 1.0f, 0.0f};
        view.CameraState.FOV = 60.0f;
        view.CameraState.NearPlane = 0.1f;
        view.CameraState.FarPlane = 100.0f;
        frame.AddView(view);
        frame.AddSceneLayer(Wayfinder::FrameLayerIds::MAIN_SCENE, 0, Wayfinder::RenderGroups::MAIN);
        frame.AddDebugLayer(Wayfinder::FrameLayerIds::DEBUG, 0);

        REQUIRE(Wayfinder::Rendering::PrepareFrame(frame, 320, 240));

        static const Wayfinder::BuiltInMeshTable K_EMPTY_MESHES{};
        const Wayfinder::FrameRenderParams params{
            .Frame = frame,
            .SwapchainWidth = 320,
            .SwapchainHeight = 240,
            .BuiltInMeshes = K_EMPTY_MESHES,
            .ResourceCache = nullptr,
            .PrimaryView = Wayfinder::Rendering::ResolvePreparedPrimaryView(frame),
        };

        Wayfinder::RenderGraph graph;
        pipeline.BuildGraph(graph, params);

        CHECK(graph.Compile());

        pipeline.Shutdown();
        context.Shutdown();
    }
} // namespace Wayfinder::Tests
