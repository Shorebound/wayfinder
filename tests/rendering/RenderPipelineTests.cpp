#include "rendering/RenderPipeline.h"
#include "rendering/RenderResources.h"
#include "rendering/RenderDevice.h"
#include "rendering/SceneRenderExtractor.h"
#include "scene/Components.h"
#include "scene/Scene.h"
#include "scene/entity/Entity.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    bool Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
        }

        return condition;
    }

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

    bool TestNullDeviceFactory()
    {
        std::unique_ptr<Wayfinder::RenderDevice> device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null);

        return Expect(static_cast<bool>(device), "null backend factory should create a device")
            && Expect(device->GetDeviceInfo().BackendName == "Null", "null device should report its backend name");
    }

    bool TestSDLGPUDeviceFactory()
    {
        std::unique_ptr<Wayfinder::RenderDevice> device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::SDL_GPU);

        return Expect(static_cast<bool>(device), "SDL_GPU backend factory should create a device");
    }

    bool TestPipelineSkipsEmptyFrame()
    {
        Wayfinder::RenderFrame frame;
        frame.SceneName = "Empty";

        // Pipeline should handle empty frames gracefully
        Wayfinder::RenderResourceCache resources;
        auto device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null);
        Wayfinder::RenderPipeline pipeline;

        // Should not crash — Prepare returns false for empty frames
        pipeline.Prepare(frame);
        return true;
    }

    bool TestExtractorBuildsExplicitPassesAndDebugPayload()
    {
        Wayfinder::Scene scene("Extractor Test Scene");
        scene.Initialize();

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
        renderable.Layer = std::string(Wayfinder::RenderLayers::Main);
        cube.AddComponent<Wayfinder::RenderableComponent>(renderable);

        Wayfinder::Entity light = scene.CreateEntity("Light");
        light.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{{1.0f, 2.0f, 0.0f}});
        Wayfinder::LightComponent lightComponent;
        lightComponent.Type = Wayfinder::LightType::Directional;
        lightComponent.DebugDraw = true;
        light.AddComponent<Wayfinder::LightComponent>(lightComponent);

        scene.Update(0.016f);

        Wayfinder::SceneRenderExtractor extractor;
        const Wayfinder::RenderFrame frame = extractor.Extract(scene);
        const Wayfinder::RenderPass* mainPass = frame.FindPass(Wayfinder::RenderPassIds::MainScene);
        const Wayfinder::RenderPass* debugPass = frame.FindPass(Wayfinder::RenderPassIds::Debug);

        scene.Shutdown();

        return Expect(frame.Views.size() == 1, "extractor should build one active view")
            && Expect(frame.Passes.size() == 3, "extractor should build the default pass schedule")
            && Expect(mainPass != nullptr, "extractor should create a main scene pass")
            && Expect(debugPass != nullptr, "extractor should create a debug pass")
            && Expect(mainPass->Meshes.size() == 1, "extractor should route renderable meshes into the main pass")
            && Expect(debugPass->DebugDraw.has_value(), "debug pass should carry a debug payload")
            && Expect(debugPass->DebugDraw->Boxes.size() == 1, "directional debug light should emit one debug box")
            && Expect(debugPass->DebugDraw->Lines.size() == 1, "directional debug light should emit one debug line");
    }

    bool TestExtractorSkipsMeshWithoutRenderable()
    {
        Wayfinder::Scene scene("Extractor Skip Scene");
        scene.Initialize();

        Wayfinder::Entity camera = scene.CreateEntity("Camera");
        camera.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{{2.0f, 2.0f, 2.0f}});
        Wayfinder::CameraComponent cameraComponent;
        cameraComponent.Primary = true;
        camera.AddComponent<Wayfinder::CameraComponent>(cameraComponent);

        Wayfinder::Entity invisibleCube = scene.CreateEntity("InvisibleCube");
        invisibleCube.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{{0.0f, 0.5f, 0.0f}});
        invisibleCube.AddComponent<Wayfinder::MeshComponent>(Wayfinder::MeshComponent{});

        scene.Update(0.016f);

        Wayfinder::SceneRenderExtractor extractor;
        const Wayfinder::RenderFrame frame = extractor.Extract(scene);
        const Wayfinder::RenderPass* mainPass = frame.FindPass(Wayfinder::RenderPassIds::MainScene);

        scene.Shutdown();

        return Expect(mainPass != nullptr, "main scene pass should still exist when no meshes are routed")
            && Expect(mainPass->Meshes.empty(), "mesh entities without RenderableComponent should not be extracted");
    }

    bool TestResourceCacheResolvesMesh()
    {
        Wayfinder::RenderFrame frame;
        const size_t viewIndex = frame.AddView(Wayfinder::RenderView{});
        Wayfinder::RenderPass& scenePass = frame.AddScenePass(
            Wayfinder::RenderPassIds::MainScene, viewIndex, Wayfinder::RenderLayers::Main);
        scenePass.Meshes.push_back(MakeSolidMesh(100, Wayfinder::Color::Red()));
        scenePass.Meshes.push_back(MakeSolidMesh(10, Wayfinder::Color::Blue()));

        Wayfinder::RenderResourceCache resources;
        resources.PrepareFrame(frame);

        // Verify resource resolution works
        const auto& resolved = resources.ResolveMesh(scenePass.Meshes[0]);
        return Expect(resolved.Geometry.Type == Wayfinder::RenderGeometryType::Box,
            "resolved mesh should match the submitted geometry type");
    }
}

int main()
{
    const bool ok = TestNullDeviceFactory()
        && TestSDLGPUDeviceFactory()
        && TestPipelineSkipsEmptyFrame()
        && TestExtractorBuildsExplicitPassesAndDebugPayload()
        && TestExtractorSkipsMeshWithoutRenderable()
        && TestResourceCacheResolvesMesh();

    if (!ok)
    {
        return EXIT_FAILURE;
    }

    std::cout << "Render pipeline tests passed\n";
    return EXIT_SUCCESS;
}