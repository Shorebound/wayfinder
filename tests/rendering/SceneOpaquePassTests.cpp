#include "app/EngineConfig.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/graph/RenderFrame.h"
#include "rendering/graph/RenderFrameUtils.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/pipeline/FrameComposer.h"
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

        Wayfinder::FrameComposer pipeline;
        pipeline.Initialise(context);

        Wayfinder::RenderFrame frame;
        Wayfinder::RenderView view;
        view.CameraState.Position = {0.0f, 0.0f, 5.0f};
        view.CameraState.Target = {0.0f, 0.0f, 0.0f};
        view.CameraState.Up = {0.0f, 1.0f, 0.0f};
        view.CameraState.FOV = 60.0f;
        view.CameraState.NearPlane = 0.1f;
        view.CameraState.FarPlane = 100.0f;
        frame.AddView(view);
        frame.AddSceneLayer(Wayfinder::FrameLayerIds::MainScene, 0, Wayfinder::RenderGroups::Main);
        frame.AddDebugLayer(Wayfinder::FrameLayerIds::Debug, 0);

        REQUIRE(pipeline.Prepare(frame, 320, 240));

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
