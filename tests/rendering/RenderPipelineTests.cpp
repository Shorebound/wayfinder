#include "TestHelpers.h"
#include "app/EngineConfig.h"
#include "assets/AssetService.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/graph/RenderFeature.h"
#include "rendering/graph/RenderFrame.h"
#include "rendering/graph/RenderFrameUtils.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/graph/SortKey.h"
#include "rendering/pipeline/RenderPipeline.h"
#include "rendering/pipeline/RenderServices.h"
#include "rendering/pipeline/SceneRenderExtractor.h"
#include "rendering/resources/RenderResources.h"
#include "scene/Components.h"
#include "scene/RuntimeComponentRegistry.h"
#include "scene/Scene.h"
#include "scene/SceneWorldBootstrap.h"
#include "scene/entity/Entity.h"

#include "ecs/Flecs.h"
#include <doctest/doctest.h>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wayfinder::Tests
{
    namespace
    {
        class OrderPass final : public Wayfinder::RenderFeature
        {
        public:
            OrderPass(std::string name, std::vector<std::string>* log) : m_name(std::move(name)), m_log(log) {}

            std::string_view GetName() const override
            {
                return m_name;
            }

            void AddPasses(Wayfinder::RenderGraph& /*graph*/, const Wayfinder::RenderPipelineFrameParams& /*params*/) override
            {
                if (m_log != nullptr)
                {
                    m_log->push_back(m_name);
                }
            }

        private:
            std::string m_name;
            std::vector<std::string>* m_log;
        };

        Wayfinder::RenderMeshSubmission MakeSolidMesh(uint8_t sortPriority, const Wayfinder::Colour& colour)
        {
            Wayfinder::RenderMeshSubmission submission;
            submission.Mesh.Origin = Wayfinder::RenderResourceOrigin::BuiltIn;
            submission.Mesh.StableKey = static_cast<uint64_t>(sortPriority) + 1ull;
            submission.Geometry.Type = Wayfinder::RenderGeometryType::Box;
            submission.Geometry.Dimensions = {1.0f, 1.0f, 1.0f};
            submission.Material.Ref.Origin = Wayfinder::RenderResourceOrigin::BuiltIn;
            submission.Material.Ref.StableKey = submission.Mesh.StableKey;
            submission.Material.StateOverrides.FillMode = Wayfinder::RenderFillMode::Solid;
            submission.Material.Parameters.SetColour("base_colour", Wayfinder::LinearColour::FromColour(colour));
            submission.SortPriority = sortPriority;
            submission.ViewIndex = 0;
            return submission;
        }
    } // namespace

    TEST_CASE("Null backend factory creates a device")
    {
        auto device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null);

        CHECK(device);
        CHECK(device->GetDeviceInfo().BackendName == "Null");
    }

    TEST_CASE("SDL_GPU backend factory creates a device")
    {
        auto device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::SDL_GPU);

        CHECK(device);
    }

    TEST_CASE("Pipeline handles empty frame without crashing")
    {
        Wayfinder::RenderFrame frame;
        frame.SceneName = "Empty";

        // PrepareFrame mutates the cache; not logically const.
        // NOLINTNEXTLINE(misc-const-correctness)
        Wayfinder::RenderResourceCache resources;
        auto device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null);
        const Wayfinder::RenderPipeline pipeline;

        // Should not crash — Prepare returns false for empty frames
        pipeline.Prepare(frame, 1280, 720);
    }

    TEST_CASE("SortKeyBuilder keeps transparent depth ordering ahead of blend grouping")
    {
        const uint64_t opaqueNear = Wayfinder::SortKeyBuilder::Build(Wayfinder::SortLayer::Opaque, 0, 7, -1.0f, 0);
        const uint64_t opaqueFar = Wayfinder::SortKeyBuilder::Build(Wayfinder::SortLayer::Opaque, 0, 7, -10.0f, 0);
        CHECK(opaqueNear < opaqueFar);

        const uint64_t transparentFar = Wayfinder::SortKeyBuilder::Build(Wayfinder::SortLayer::Transparent, 1, 3, -10.0f, 0);
        const uint64_t transparentNear = Wayfinder::SortKeyBuilder::Build(Wayfinder::SortLayer::Transparent, 63, 1024, -1.0f, 0);
        CHECK(transparentFar < transparentNear);
    }

    TEST_CASE("Extractor builds explicit passes and debug payload")
    {
        flecs::world world;
        Wayfinder::Scene::RegisterCoreComponents(world);
        Wayfinder::RuntimeComponentRegistry registry;
        registry.AddCoreEntries();
        registry.RegisterComponents(world);
        Wayfinder::SceneWorldBootstrap::RegisterDefaultScenePlugins(world);
        Wayfinder::Scene scene(world, registry, "Extractor Test Scene");

        Wayfinder::Entity camera = scene.CreateEntity("Camera");
        camera.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{{4.0f, 3.0f, 4.0f}});
        Wayfinder::CameraComponent cameraComponent;
        cameraComponent.Primary = true;
        cameraComponent.Target = {0.0f, 0.5f, 0.0f};
        camera.AddComponent<Wayfinder::CameraComponent>(cameraComponent);

        Wayfinder::Entity cube = scene.CreateEntity("Cube");
        cube.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{{0.0f, 0.5f, 0.0f}});
        cube.AddComponent<Wayfinder::MeshComponent>(Wayfinder::MeshComponent{});
        Wayfinder::RenderableComponent renderable;
        renderable.Layer = Wayfinder::RenderLayers::Main;
        cube.AddComponent<Wayfinder::RenderableComponent>(renderable);

        Wayfinder::Entity light = scene.CreateEntity("Light");
        light.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{{1.0f, 2.0f, 0.0f}});
        Wayfinder::LightComponent lightComponent;
        lightComponent.Type = Wayfinder::LightType::Directional;
        lightComponent.DebugDraw = true;
        light.AddComponent<Wayfinder::LightComponent>(lightComponent);

        world.progress(0.016f);

        const Wayfinder::SceneRenderExtractor extractor;
        const Wayfinder::RenderFrame frame = extractor.Extract(scene);
        const Wayfinder::FrameLayerRecord* mainPass = frame.FindLayer(Wayfinder::FrameLayerIds::MainScene);
        const Wayfinder::FrameLayerRecord* debugPass = frame.FindLayer(Wayfinder::FrameLayerIds::Debug);

        scene.Shutdown();

        CHECK(frame.Views.size() == 1);
        CHECK(frame.Layers.size() == 3);
        REQUIRE(mainPass != nullptr);
        REQUIRE(debugPass != nullptr);
        CHECK(mainPass->Meshes.size() == 1);
        REQUIRE(debugPass->DebugDraw.has_value());
        const Wayfinder::RenderDebugDrawList& debugDraw = *debugPass->DebugDraw;
        CHECK(debugDraw.Boxes.size() == 1);
        CHECK(debugDraw.Lines.size() == 1);
    }

    TEST_CASE("Extractor skips mesh without renderable")
    {
        flecs::world world;
        Wayfinder::Scene::RegisterCoreComponents(world);
        Wayfinder::RuntimeComponentRegistry registry;
        registry.AddCoreEntries();
        registry.RegisterComponents(world);
        Wayfinder::SceneWorldBootstrap::RegisterDefaultScenePlugins(world);
        Wayfinder::Scene scene(world, registry, "Extractor Skip Scene");

        Wayfinder::Entity camera = scene.CreateEntity("Camera");
        camera.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{{2.0f, 2.0f, 2.0f}});
        Wayfinder::CameraComponent cameraComponent;
        cameraComponent.Primary = true;
        camera.AddComponent<Wayfinder::CameraComponent>(cameraComponent);

        Wayfinder::Entity invisibleCube = scene.CreateEntity("InvisibleCube");
        invisibleCube.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{{0.0f, 0.5f, 0.0f}});
        invisibleCube.AddComponent<Wayfinder::MeshComponent>(Wayfinder::MeshComponent{});

        world.progress(0.016f);

        const Wayfinder::SceneRenderExtractor extractor;
        const Wayfinder::RenderFrame frame = extractor.Extract(scene);
        const Wayfinder::FrameLayerRecord* mainPass = frame.FindLayer(Wayfinder::FrameLayerIds::MainScene);

        scene.Shutdown();

        REQUIRE(mainPass != nullptr);
        CHECK(mainPass->Meshes.empty());
    }

    TEST_CASE("Extractor routes blended materials on main layer into transparent sorting")
    {
        auto blendedMaterialId = Wayfinder::AssetId::Parse("a0000000-0000-0000-0000-0000000000f1");
        REQUIRE(blendedMaterialId.has_value());

        flecs::world world;
        Wayfinder::Scene::RegisterCoreComponents(world);
        Wayfinder::RuntimeComponentRegistry registry;
        registry.AddCoreEntries();
        registry.RegisterComponents(world);
        Wayfinder::SceneWorldBootstrap::RegisterDefaultScenePlugins(world);
        Wayfinder::Scene scene(world, registry, "Extractor Transparent Sort Scene");

        auto assetService = std::make_shared<Wayfinder::AssetService>();
        std::string assetError;
        REQUIRE(assetService->SetAssetRoot(Wayfinder::Tests::Helpers::FixturesDir() / "blend_test", assetError));
        scene.SetAssetService(assetService);

        Wayfinder::Entity camera = scene.CreateEntity("Camera");
        camera.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{{0.0f, 0.0f, 5.0f}});
        Wayfinder::CameraComponent cameraComponent;
        cameraComponent.Primary = true;
        cameraComponent.Target = {0.0f, 0.0f, 0.0f};
        camera.AddComponent<Wayfinder::CameraComponent>(cameraComponent);

        auto createBlendedCube = [&](const std::string& name, float worldZ)
        {
            Wayfinder::Entity cube = scene.CreateEntity(name);
            cube.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{{0.0f, 0.5f, worldZ}});
            cube.AddComponent<Wayfinder::MeshComponent>(Wayfinder::MeshComponent{});

            Wayfinder::RenderableComponent renderable;
            renderable.Layer = Wayfinder::RenderLayers::Main;
            cube.AddComponent<Wayfinder::RenderableComponent>(renderable);

            Wayfinder::MaterialComponent material;
            material.MaterialAssetId = *blendedMaterialId;
            cube.AddComponent<Wayfinder::MaterialComponent>(material);
        };

        createBlendedCube("FarCube", 0.0f);
        createBlendedCube("NearCube", 2.0f);

        world.progress(0.016f);

        const Wayfinder::SceneRenderExtractor extractor;
        Wayfinder::RenderFrame frame = extractor.Extract(scene);
        Wayfinder::FrameLayerRecord* mainPass = frame.FindLayer(Wayfinder::FrameLayerIds::MainScene);

        REQUIRE(mainPass != nullptr);
        REQUIRE(mainPass->Meshes.size() == 2);
        CHECK((mainPass->Meshes[0].SortKey >> 62) == static_cast<uint64_t>(Wayfinder::SortLayer::Transparent));
        CHECK((mainPass->Meshes[1].SortKey >> 62) == static_cast<uint64_t>(Wayfinder::SortLayer::Transparent));

        const Wayfinder::RenderPipeline pipeline;
        REQUIRE(pipeline.Prepare(frame, 1280, 720));

        mainPass = frame.FindLayer(Wayfinder::FrameLayerIds::MainScene);
        REQUIRE(mainPass != nullptr);
        REQUIRE(mainPass->Meshes.size() == 2);
        CHECK(mainPass->Meshes[0].LocalToWorld[3].z == doctest::Approx(0.0f));
        CHECK(mainPass->Meshes[1].LocalToWorld[3].z == doctest::Approx(2.0f));

        scene.Shutdown();
    }

    TEST_CASE("Resource cache resolves mesh")
    {
        Wayfinder::RenderFrame frame;
        const size_t viewIndex = frame.AddView(Wayfinder::RenderView{});
        Wayfinder::FrameLayerRecord& scenePass = frame.AddSceneLayer(Wayfinder::FrameLayerIds::MainScene, viewIndex, Wayfinder::RenderLayers::Main);
        scenePass.Meshes.push_back(MakeSolidMesh(100, Wayfinder::Colour::Red()));
        scenePass.Meshes.push_back(MakeSolidMesh(10, Wayfinder::Colour::Blue()));

        Wayfinder::RenderResourceCache resources;
        resources.PrepareFrame(frame);

        REQUIRE(!scenePass.Meshes.empty());
        const auto& resolved = resources.ResolveMesh(scenePass.Meshes.at(0));
        CHECK(resolved.Geometry.Type == Wayfinder::RenderGeometryType::Box);
    }

    TEST_CASE("RenderPipeline::Initialise registers built-in programs")
    {
        auto device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null);
        REQUIRE(device);

        Wayfinder::EngineConfig config;
        config.Window.Width = 320;
        config.Window.Height = 240;

        Wayfinder::RenderServices context;
        REQUIRE(context.Initialise(*device, config));

        Wayfinder::RenderPipeline pipeline;
        // Initialise must not crash — it registers programs via the context.
        // With NullDevice, pipeline creation fails (no shader files on disk),
        // so Find returns nullptr. The contract being tested is that Initialise
        // calls Register for each built-in program without crashing.
        pipeline.Initialise(context);

        pipeline.Shutdown();
        context.Shutdown();
    }

    TEST_CASE("RenderPipeline::Shutdown is safe after Initialise")
    {
        auto device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null);
        REQUIRE(device);

        Wayfinder::EngineConfig config;
        config.Window.Width = 320;
        config.Window.Height = 240;

        Wayfinder::RenderServices context;
        REQUIRE(context.Initialise(*device, config));

        Wayfinder::RenderPipeline pipeline;
        pipeline.Initialise(context);
        pipeline.Shutdown();
        pipeline.Shutdown(); // Double shutdown is safe

        context.Shutdown();
    }

    TEST_CASE("RenderPipeline orders RegisterPass by phase then order then registration sequence")
    {
        auto device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null);
        REQUIRE(device);

        Wayfinder::EngineConfig config;
        config.Window.Width = 320;
        config.Window.Height = 240;

        Wayfinder::RenderServices context;
        REQUIRE(context.Initialise(*device, config));

        std::vector<std::string> order;
        Wayfinder::RenderPipeline pipeline;
        pipeline.Initialise(context);

        pipeline.RegisterPass(Wayfinder::RenderPhase::PostProcess, 100, std::make_unique<OrderPass>("p100", &order));
        pipeline.RegisterPass(Wayfinder::RenderPhase::Opaque, 0, std::make_unique<OrderPass>("o0", &order));
        pipeline.RegisterPass(Wayfinder::RenderPhase::Present, 0, std::make_unique<OrderPass>("pr0", &order));
        pipeline.RegisterPass(Wayfinder::RenderPhase::PostProcess, 0, std::make_unique<OrderPass>("p0", &order));

        Wayfinder::RenderFrame frame;
        Wayfinder::RenderView view;
        view.CameraState.Position = {0.0f, 0.0f, 5.0f};
        view.CameraState.Target = {0.0f, 0.0f, 0.0f};
        view.CameraState.Up = {0.0f, 1.0f, 0.0f};
        view.CameraState.FOV = 60.0f;
        view.CameraState.NearPlane = 0.1f;
        view.CameraState.FarPlane = 100.0f;
        frame.AddView(view);
        frame.AddSceneLayer(Wayfinder::FrameLayerIds::MainScene, 0, Wayfinder::RenderLayers::Main);
        frame.AddDebugLayer(Wayfinder::FrameLayerIds::Debug, 0);

        REQUIRE(pipeline.Prepare(frame, 320, 240));

        static const std::unordered_map<uint32_t, Wayfinder::Mesh*> K_EMPTY_MESHES;
        const Wayfinder::RenderPipelineFrameParams params{
            .Frame = frame,
            .SwapchainWidth = 320,
            .SwapchainHeight = 240,
            .MeshesByStride = K_EMPTY_MESHES,
            .ResourceCache = nullptr,
            .PrimaryView = Wayfinder::Rendering::ResolvePreparedPrimaryView(frame),
        };

        Wayfinder::RenderGraph graph;
        pipeline.BuildGraph(graph, params);

        auto indexOf = [&](const std::string& name) -> size_t
        {
            const auto it = std::ranges::find(order, name);
            REQUIRE(it != order.end());
            return static_cast<size_t>(std::distance(order.begin(), it));
        };

        CHECK(indexOf("o0") < indexOf("p0"));
        CHECK(indexOf("p0") < indexOf("p100"));
        CHECK(indexOf("p100") < indexOf("pr0"));

        pipeline.Shutdown();
        context.Shutdown();
    }

    TEST_CASE("RenderPipeline ties same phase and order by registration sequence")
    {
        auto device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null);
        REQUIRE(device);

        Wayfinder::EngineConfig config;
        config.Window.Width = 320;
        config.Window.Height = 240;

        Wayfinder::RenderServices context;
        REQUIRE(context.Initialise(*device, config));

        std::vector<std::string> order;
        Wayfinder::RenderPipeline pipeline;
        pipeline.Initialise(context);

        pipeline.RegisterPass(Wayfinder::RenderPhase::PostProcess, 0, std::make_unique<OrderPass>("first", &order));
        pipeline.RegisterPass(Wayfinder::RenderPhase::PostProcess, 0, std::make_unique<OrderPass>("second", &order));

        Wayfinder::RenderFrame frame;
        Wayfinder::RenderView view;
        view.CameraState.Position = {0.0f, 0.0f, 5.0f};
        view.CameraState.Target = {0.0f, 0.0f, 0.0f};
        view.CameraState.Up = {0.0f, 1.0f, 0.0f};
        view.CameraState.FOV = 60.0f;
        view.CameraState.NearPlane = 0.1f;
        view.CameraState.FarPlane = 100.0f;
        frame.AddView(view);
        frame.AddSceneLayer(Wayfinder::FrameLayerIds::MainScene, 0, Wayfinder::RenderLayers::Main);
        frame.AddDebugLayer(Wayfinder::FrameLayerIds::Debug, 0);

        REQUIRE(pipeline.Prepare(frame, 320, 240));

        static const std::unordered_map<uint32_t, Wayfinder::Mesh*> K_EMPTY_MESHES;
        const Wayfinder::RenderPipelineFrameParams params{
            .Frame = frame,
            .SwapchainWidth = 320,
            .SwapchainHeight = 240,
            .MeshesByStride = K_EMPTY_MESHES,
            .ResourceCache = nullptr,
            .PrimaryView = Wayfinder::Rendering::ResolvePreparedPrimaryView(frame),
        };

        Wayfinder::RenderGraph graph;
        pipeline.BuildGraph(graph, params);

        const auto firstIt = std::ranges::find(order, "first");
        const auto secondIt = std::ranges::find(order, "second");
        REQUIRE(firstIt != order.end());
        REQUIRE(secondIt != order.end());
        CHECK(firstIt < secondIt);

        pipeline.Shutdown();
        context.Shutdown();
    }
}
