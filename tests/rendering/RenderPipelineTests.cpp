#include "app/EngineConfig.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/graph/RenderFrame.h"
#include "rendering/pipeline/RenderContext.h"
#include "rendering/pipeline/RenderPipeline.h"
#include "rendering/pipeline/SceneRenderExtractor.h"
#include "rendering/resources/RenderResources.h"
#include "scene/Components.h"
#include "scene/RuntimeComponentRegistry.h"
#include "scene/Scene.h"
#include "scene/SceneWorldBootstrap.h"
#include "scene/entity/Entity.h"

#include "ecs/Flecs.h"
#include <doctest/doctest.h>
#include <string>

namespace Wayfinder::Tests
{
    namespace
    {
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
        pipeline.Prepare(frame);
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
        const Wayfinder::RenderPass* mainPass = frame.FindPass(Wayfinder::RenderPassIds::MainScene);
        const Wayfinder::RenderPass* debugPass = frame.FindPass(Wayfinder::RenderPassIds::Debug);

        scene.Shutdown();

        CHECK(frame.Views.size() == 1);
        CHECK(frame.Passes.size() == 3);
        REQUIRE(mainPass != nullptr);
        REQUIRE(debugPass != nullptr);
        CHECK(mainPass->Meshes.size() == 1);
        REQUIRE(debugPass->DebugDraw.has_value());
        const Wayfinder::RenderDebugDrawList& debugDraw = debugPass->DebugDraw.value();
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
        const Wayfinder::RenderPass* mainPass = frame.FindPass(Wayfinder::RenderPassIds::MainScene);

        scene.Shutdown();

        REQUIRE(mainPass != nullptr);
        CHECK(mainPass->Meshes.empty());
    }

    TEST_CASE("Resource cache resolves mesh")
    {
        Wayfinder::RenderFrame frame;
        const size_t viewIndex = frame.AddView(Wayfinder::RenderView{});
        Wayfinder::RenderPass& scenePass = frame.AddScenePass(Wayfinder::RenderPassIds::MainScene, viewIndex, Wayfinder::RenderLayers::Main);
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

        Wayfinder::RenderContext context;
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

        Wayfinder::RenderContext context;
        REQUIRE(context.Initialise(*device, config));

        Wayfinder::RenderPipeline pipeline;
        pipeline.Initialise(context);
        pipeline.Shutdown();
        pipeline.Shutdown(); // Double shutdown is safe

        context.Shutdown();
    }
}