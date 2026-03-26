#include "app/EngineConfig.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/graph/RenderFrame.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/pipeline/RenderContext.h"
#include "rendering/pipeline/RenderPipeline.h"

#include <doctest/doctest.h>

#include <unordered_map>

namespace Wayfinder::Tests
{
    TEST_CASE("SceneOpaquePass: BuildGraph compiles with empty game passes")
    {
        auto device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null);
        REQUIRE(device);

        Wayfinder::EngineConfig config;
        config.Window.Width = 320;
        config.Window.Height = 240;

        Wayfinder::RenderContext context;
        REQUIRE(context.Initialise(*device, config));

        Wayfinder::RenderPipeline pipeline;
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
        frame.AddScenePass(Wayfinder::RenderPassIds::MainScene, 0, Wayfinder::RenderLayers::Main);
        frame.AddDebugPass(Wayfinder::RenderPassIds::Debug, 0);

        REQUIRE(pipeline.Prepare(frame, 320, 240));

        static const std::unordered_map<uint32_t, Wayfinder::Mesh*> K_EMPTY_MESHES;
        const Wayfinder::RenderPipelineFrameParams params{
            .Frame = frame,
            .SwapchainWidth = 320,
            .SwapchainHeight = 240,
            .MeshesByStride = K_EMPTY_MESHES,
            .ResourceCache = nullptr,
        };

        Wayfinder::RenderGraph graph;
        pipeline.BuildGraph(graph, params, {});

        CHECK(graph.Compile());

        pipeline.Shutdown();
        context.Shutdown();
    }
} // namespace Wayfinder::Tests
