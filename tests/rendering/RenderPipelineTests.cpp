#include "rendering/RenderPipeline.h"
#include "rendering/RenderResources.h"
#include "rendering/GraphicsContext.h"
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
    struct DrawCall
    {
        std::string Kind;
        Wayfinder::Color Color{};
    };

    class FakeRenderAPI final : public Wayfinder::IRenderAPI
    {
    public:
        explicit FakeRenderAPI(Wayfinder::RenderBackendCapabilities capabilities)
            : m_capabilities(std::move(capabilities))
        {
        }

        void Initialize() override {}
        void Shutdown() override {}
        const Wayfinder::RenderBackendCapabilities& GetCapabilities() const override { return m_capabilities; }

        void DrawText(const std::string&, int, int, int, const Wayfinder::Color&) override {}
        void DrawFPS(int, int) override {}

        void Begin3DMode(const Wayfinder::Camera&) override { ++m_begin3DCalls; }
        void End3DMode() override { ++m_end3DCalls; }
        void DrawGrid(int, float) override { ++m_gridCalls; }

        void DrawBox(const Wayfinder::Matrix4&, const Wayfinder::Float3&, const Wayfinder::Color& color) override
        {
            m_drawCalls.push_back({"box", color});
        }

        void DrawBoxWires(const Wayfinder::Matrix4&, const Wayfinder::Float3&, const Wayfinder::Color& color) override
        {
            m_drawCalls.push_back({"wire", color});
        }

        void DrawLine3D(const Wayfinder::Float3&, const Wayfinder::Float3&, const Wayfinder::Color& color) override
        {
            m_drawCalls.push_back({"line", color});
        }

        int Begin3DCalls() const { return m_begin3DCalls; }
        int End3DCalls() const { return m_end3DCalls; }
        int GridCalls() const { return m_gridCalls; }
        const std::vector<DrawCall>& DrawCalls() const { return m_drawCalls; }

    private:
        Wayfinder::RenderBackendCapabilities m_capabilities;
        int m_begin3DCalls = 0;
        int m_end3DCalls = 0;
        int m_gridCalls = 0;
        std::vector<DrawCall> m_drawCalls;
    };

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
        submission.Material.FillMode = Wayfinder::RenderFillMode::Solid;
        submission.Material.BaseColor = color;
        submission.Material.HasBaseColorOverride = true;
        submission.SortPriority = sortPriority;
        return submission;
    }

    bool TestScenePassUsesPassOwnedSubmissions()
    {
        Wayfinder::RenderFrame frame;
        const size_t viewIndex = frame.AddView(Wayfinder::RenderView{});
        Wayfinder::RenderPass& scenePass = frame.AddScenePass(Wayfinder::RenderPassIds::MainScene, viewIndex, Wayfinder::RenderLayers::Main);
        scenePass.Meshes.push_back(MakeSolidMesh(100, Wayfinder::Color::Red()));
        scenePass.Meshes.push_back(MakeSolidMesh(10, Wayfinder::Color::Blue()));

        Wayfinder::RenderResourceCache resources;
        resources.PrepareFrame(frame);

        FakeRenderAPI api({.BackendName = "Test", .MaxViewCount = 1, .SupportsScenePasses = true, .SupportsDebugPasses = true, .SupportsRenderTargets = false, .SupportsBoxGeometry = true, .SupportsDebugLines = true});
        Wayfinder::RenderPipeline pipeline;
        pipeline.Execute(frame, api, resources);

        const auto& draws = api.DrawCalls();
        return Expect(draws.size() == 2, "scene pass should emit two draw calls")
            && Expect(draws[0].Color.b == 255, "scene pass should sort lower sort-priority submissions first")
            && Expect(draws[1].Color.r == 255, "scene pass should draw the second submission after sorting");
    }

    bool TestBackendViewLimitSkipsUnsupportedPasses()
    {
        Wayfinder::RenderFrame frame;
        frame.AddView(Wayfinder::RenderView{});
        frame.AddView(Wayfinder::RenderView{});

        Wayfinder::RenderPass& firstPass = frame.AddScenePass("view0", 0, Wayfinder::RenderLayers::Main);
        firstPass.Meshes.push_back(MakeSolidMesh(1, Wayfinder::Color::Green()));

        Wayfinder::RenderPass& secondPass = frame.AddScenePass("view1", 1, Wayfinder::RenderLayers::Main);
        secondPass.Meshes.push_back(MakeSolidMesh(2, Wayfinder::Color::Red()));

        Wayfinder::RenderResourceCache resources;
        resources.PrepareFrame(frame);

        FakeRenderAPI api({.BackendName = "RaylibLike", .MaxViewCount = 1, .SupportsScenePasses = true, .SupportsDebugPasses = true, .SupportsRenderTargets = false, .SupportsBoxGeometry = true, .SupportsDebugLines = true});
        Wayfinder::RenderPipeline pipeline;
        pipeline.Execute(frame, api, resources);

        const auto& draws = api.DrawCalls();
        return Expect(draws.size() == 1, "unsupported secondary view pass should be skipped")
            && Expect(draws[0].Color.g == 255, "supported primary view pass should still draw");
    }

    bool TestDebugLineCapabilityIsEnforced()
    {
        Wayfinder::RenderFrame frame;
        const size_t viewIndex = frame.AddView(Wayfinder::RenderView{});
        Wayfinder::RenderPass& debugPass = frame.AddDebugPass(Wayfinder::RenderPassIds::Debug, viewIndex);
        debugPass.DebugDraw->ShowWorldGrid = true;
        debugPass.DebugDraw->Lines.push_back({{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, Wayfinder::Color::Yellow()});

        Wayfinder::RenderDebugBox debugBox;
        debugBox.Material.FillMode = Wayfinder::RenderFillMode::Solid;
        debugBox.Material.BaseColor = Wayfinder::Color::White();
        debugBox.Material.HasBaseColorOverride = true;
        debugPass.DebugDraw->Boxes.push_back(debugBox);

        Wayfinder::RenderResourceCache resources;
        resources.PrepareFrame(frame);

        FakeRenderAPI api({.BackendName = "NoLineBackend", .MaxViewCount = 1, .SupportsScenePasses = true, .SupportsDebugPasses = true, .SupportsRenderTargets = false, .SupportsBoxGeometry = true, .SupportsDebugLines = false});
        Wayfinder::RenderPipeline pipeline;
        pipeline.Execute(frame, api, resources);

        const auto& draws = api.DrawCalls();
        return Expect(api.GridCalls() == 1, "debug pass should still draw supported grid output")
            && Expect(draws.size() == 1, "debug pass should skip unsupported lines but keep supported boxes")
            && Expect(draws[0].Kind == "box", "remaining debug draw should be the box");
    }

    bool TestNullBackendFactories()
    {
        std::unique_ptr<Wayfinder::IRenderAPI> renderAPI = Wayfinder::IRenderAPI::Create(Wayfinder::RenderBackend::Null);
        std::unique_ptr<Wayfinder::IGraphicsContext> graphicsContext = Wayfinder::IGraphicsContext::Create(Wayfinder::RenderBackend::Null);

        return Expect(static_cast<bool>(renderAPI), "null render backend factory should create a render API")
            && Expect(static_cast<bool>(graphicsContext), "null render backend factory should create a graphics context")
            && Expect(renderAPI->GetCapabilities().BackendName == "Null", "null render backend should report its backend name")
            && Expect(graphicsContext->GetCapabilities().BackendName == "Null", "null graphics context should report its backend name");
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
}

int main()
{
    const bool ok = TestScenePassUsesPassOwnedSubmissions()
        && TestBackendViewLimitSkipsUnsupportedPasses()
        && TestDebugLineCapabilityIsEnforced()
        && TestNullBackendFactories()
        && TestExtractorBuildsExplicitPassesAndDebugPayload()
        && TestExtractorSkipsMeshWithoutRenderable();

    if (!ok)
    {
        return EXIT_FAILURE;
    }

    std::cout << "Render pipeline tests passed\n";
    return EXIT_SUCCESS;
}