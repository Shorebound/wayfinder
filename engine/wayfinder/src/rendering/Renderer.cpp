#include "Renderer.h"
#include "../core/ServiceLocator.h"
#include "../scene/Scene.h"
#include "../scene/entity/Entity.h"
#include "../scene/Components.h"
#include "../rendering/RenderAPI.h"
#include "../rendering/GraphicsContext.h"

namespace Wayfinder
{
    Renderer::Renderer()
        : m_screenWidth(800), m_screenHeight(450), m_isInitialized(false)
    {
        // Initialize camera with default values
        m_camera.Position = {.x = 0.0f, .y = 10.0f, .z = 10.0f};
        m_camera.Target = {.x = 0.0f, .y = 0.0f, .z = 0.0f};
        m_camera.Up = {.x = 0.0f, .y = 1.0f, .z = 0.0f};
        m_camera.FOV = 45.0f;
        m_camera.ProjectionType = 0; // CAMERA_PERSPECTIVE
    }

    Renderer::~Renderer()
    {
        if (m_isInitialized)
        {
            Shutdown();
        }
    }

    bool Renderer::Initialize(int screenWidth, int screenHeight)
    {
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;

        // Additional renderer initialization can go here
        // For example, loading shaders, creating render targets, etc.

        m_isInitialized = true;
        return true;
    }

    void Renderer::Shutdown()
    {
        m_isInitialized = false;
    }

    void Renderer::Render(const Scene& scene)
    {
        if (!m_isInitialized)
            return;

        SyncSceneCamera(scene);

        BeginFrame();

        RenderEntities(scene);
        RenderLights(scene);

        // Get render API from service locator
        auto& renderAPI = ServiceLocator::GetRenderAPI();

        // Draw scene info
        renderAPI.DrawText("Scene: " + scene.GetName(), 10, 30, 20, Color::DarkGray());

        EndFrame();
    }

    void Renderer::SetCameraPosition(float x, float y, float z)
    {
        m_camera.Position = {.x = x, .y = y, .z = z};
    }

    void Renderer::SetCameraTarget(float x, float y, float z)
    {
        m_camera.Target = {.x = x, .y = y, .z = z};
    }

    void Renderer::SyncSceneCamera(const Scene& scene)
    {
        if (!scene.GetWorld().has<ActiveCameraStateComponent>())
        {
            return;
        }

        const ActiveCameraStateComponent& activeCamera = scene.GetWorld().get<ActiveCameraStateComponent>();
        if (!activeCamera.IsValid)
        {
            return;
        }

        m_camera.Position = { activeCamera.Position.x, activeCamera.Position.y, activeCamera.Position.z };
        m_camera.Target = { activeCamera.Target.x, activeCamera.Target.y, activeCamera.Target.z };
        m_camera.Up = { activeCamera.Up.x, activeCamera.Up.y, activeCamera.Up.z };
        m_camera.FOV = activeCamera.FieldOfView;
        m_camera.ProjectionType = static_cast<int>(activeCamera.Projection);
    }

    void Renderer::BeginFrame()
    {
        auto& graphicsContext = ServiceLocator::GetGraphicsContext();
        //auto& renderAPI = ServiceLocator::GetRenderAPI();

        graphicsContext.BeginFrame();
        graphicsContext.Clear(1.0f, 1.0f, 1.0f); // White background
    }

    void Renderer::EndFrame()
    {
        auto& renderAPI = ServiceLocator::GetRenderAPI();
        auto& graphicsContext = ServiceLocator::GetGraphicsContext();

        renderAPI.DrawFPS(10, 10);
        graphicsContext.EndFrame();
    }

    void Renderer::RenderEntities(const Scene& scene)
    {
        auto& renderAPI = ServiceLocator::GetRenderAPI();

        renderAPI.Begin3DMode(m_camera);
        renderAPI.DrawGrid(100, 1.0f);

        // Query renderable entities explicitly instead of assuming every transform should draw.
        scene.GetWorld().each([&](flecs::entity entityHandle, const TransformComponent& transform, const MeshComponent& mesh)
        {
            Vector3 pos = transform.Position;
            Vector3 size = Vector3Multiply(mesh.Dimensions, transform.Scale);
            Color tint = mesh.Albedo;
            bool wireframe = mesh.Wireframe;
            if (entityHandle.has<WorldTransformComponent>())
            {
                const auto& worldTransform = entityHandle.get<WorldTransformComponent>();
                pos = worldTransform.Position;
                size = Vector3Multiply(mesh.Dimensions, worldTransform.Scale);
            }

            if (entityHandle.has<MaterialComponent>())
            {
                const auto& material = entityHandle.get<MaterialComponent>();
                tint = material.BaseColor;
                wireframe = material.Wireframe;
            }

            switch (mesh.Primitive)
            {
            case MeshPrimitive::Cube:
                renderAPI.DrawCube(pos.x, pos.y, pos.z, size.x, size.y, size.z, tint);
                if (wireframe)
                {
                    renderAPI.DrawCubeWires(pos.x, pos.y, pos.z, size.x, size.y, size.z, Color::DarkGray());
                }
                break;
            }
        });

        renderAPI.End3DMode();
    }

    void Renderer::RenderLights(const Scene& scene)
    {
        auto& renderAPI = ServiceLocator::GetRenderAPI();

        renderAPI.Begin3DMode(m_camera);
        scene.GetWorld().each([&](flecs::entity entityHandle, const TransformComponent& transform, const LightComponent& light)
        {
            if (!light.DebugDraw)
            {
                return;
            }

            Vector3 pos = transform.Position;
            if (entityHandle.has<WorldTransformComponent>())
            {
                pos = entityHandle.get<WorldTransformComponent>().Position;
            }

            const float debugSize = light.Type == LightType::Directional ? 0.6f : 0.3f;
            renderAPI.DrawCube(
                pos.x,
                pos.y,
                pos.z,
                debugSize,
                debugSize,
                debugSize,
                light.Tint);
            renderAPI.DrawCubeWires(
                pos.x,
                pos.y,
                pos.z,
                debugSize,
                debugSize,
                debugSize,
                Color::DarkGray());
        });
        renderAPI.End3DMode();
    }
} // namespace Wayfinder
