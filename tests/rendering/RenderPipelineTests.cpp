#include "rendering/RenderPipeline.h"
#include "rendering/RenderResources.h"
#include "rendering/RenderDevice.h"
#include "rendering/SceneRenderExtractor.h"
#include "scene/Components.h"
#include "scene/RuntimeComponentRegistry.h"
#include "scene/Scene.h"
#include "scene/entity/Entity.h"

#include <doctest/doctest.h>
#include <flecs.h>
#include <string>

namespace
{
    Wayfinder::RenderMeshSubmission MakeSolidMesh(uint8_t sortPriority, const Wayfinder::Color& color)
    {
        Wayfinder::RenderMeshSubmission submission;
        submission.Mesh.Origin = Wayfinder::RenderResourceOrigin::BuiltIn;
        submission.Mesh.StableKey = static_cast<uint64_t>(sortPriority) + 1ull;
        submission.Geometry.Type = Wayfinder::RenderGeometryType::Box;
        submission.Geometry.Dimensions = {1.0f, 1.0f, 1.0f};
        submission.Material.Handle.Origin = Wayfinder::RenderResourceOrigin::BuiltIn;
        submission.Material.Handle.StableKey = submission.Mesh.StableKey;
        submission.Material.StateOverrides.FillMode = Wayfinder::RenderFillMode::Solid;
        submission.Material.Parameters.SetColor("base_color", Wayfinder::LinearColor::FromColor(color));
        submission.SortPriority = sortPriority;
        return submission;
    }
}

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

    Wayfinder::RenderResourceCache resources;
    auto device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null);
    Wayfinder::RenderPipeline pipeline;

    // Should not crash — Prepare returns false for empty frames
    pipeline.Prepare(frame);
}

TEST_CASE("Extractor builds explicit passes and debug payload")
{
    flecs::world world;
    Wayfinder::Scene::RegisterCoreECS(world);
    Wayfinder::RuntimeComponentRegistry registry;
    registry.AddCoreEntries();
    registry.RegisterComponents(world);
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

    Wayfinder::SceneRenderExtractor extractor;
    const Wayfinder::RenderFrame frame = extractor.Extract(scene);
    const Wayfinder::RenderPass* mainPass = frame.FindPass(Wayfinder::RenderPassIds::MainScene);
    const Wayfinder::RenderPass* debugPass = frame.FindPass(Wayfinder::RenderPassIds::Debug);

    scene.Shutdown();

    CHECK(frame.Views.size() == 1);
    CHECK(frame.Passes.size() == 3);
    REQUIRE(mainPass != nullptr);
    REQUIRE(debugPass != nullptr);
    CHECK(mainPass->Meshes.size() == 1);
    CHECK(debugPass->DebugDraw.has_value());
    CHECK(debugPass->DebugDraw->Boxes.size() == 1);
    CHECK(debugPass->DebugDraw->Lines.size() == 1);
}

TEST_CASE("Extractor skips mesh without renderable")
{
    flecs::world world;
    Wayfinder::Scene::RegisterCoreECS(world);
    Wayfinder::RuntimeComponentRegistry registry;
    registry.AddCoreEntries();
    registry.RegisterComponents(world);
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

    Wayfinder::SceneRenderExtractor extractor;
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
    Wayfinder::RenderPass& scenePass =
        frame.AddScenePass(Wayfinder::RenderPassIds::MainScene, viewIndex, Wayfinder::RenderLayers::Main);
    scenePass.Meshes.push_back(MakeSolidMesh(100, Wayfinder::Color::Red()));
    scenePass.Meshes.push_back(MakeSolidMesh(10, Wayfinder::Color::Blue()));

    Wayfinder::RenderResourceCache resources;
    resources.PrepareFrame(frame);

    const auto& resolved = resources.ResolveMesh(scenePass.Meshes[0]);
    CHECK(resolved.Geometry.Type == Wayfinder::RenderGeometryType::Box);
}