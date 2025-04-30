#include "Renderer.h"
#include "../core/ServiceLocator.h"
#include "../scene/Scene.h"
#include "../scene/entity/Entity.h"
#include "../scene/entity/Transform.h"
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

        BeginFrame();

        RenderEntities(scene);

        // Get render API from service locator
        auto& renderAPI = ServiceLocator::GetRenderAPI();

        // Draw scene info
        renderAPI.DrawText("Scene: " + scene.GetName(), 10, 30, 20, Color::DarkGray());
        renderAPI.DrawText("Entities: " + std::to_string(scene.GetEntityCount()), 10, 50, 20, Color::DarkGray());

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
        const auto entities = scene.GetAllEntities();
        auto& renderAPI = ServiceLocator::GetRenderAPI();

        renderAPI.Begin3DMode(m_camera);
        renderAPI.DrawGrid(100, 1.0f);

        for (const auto& entity : entities)
        {
            if (entity->IsActive())
            {
                if (const auto transform = entity->GetTransform())
                {
                    const auto [x, y, z] = transform->GetPosition();

                    // For now, just draw a cube at the entity's position
                    // Later we'll implement proper model rendering
                    renderAPI.DrawCube(x, y, z, 1.0f, 1.0f, 1.0f, Color::Red());
                    renderAPI.DrawCubeWires(x, y, z, 1.0f, 1.0f, 1.0f, Color::DarkGray());
                }

                // Let the entity render itself
                entity->Render();
            }
        }

        renderAPI.End3DMode();
    }
} // namespace Wayfinder
