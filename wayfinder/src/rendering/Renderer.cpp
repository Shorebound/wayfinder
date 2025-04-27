#include "Renderer.h"
#include "../scene/Scene.h"
#include "../scene/entity/Entity.h"
#include "../scene/entity/Transform.h"

namespace Wayfinder
{

    Renderer::Renderer()
        : m_screenWidth(800), m_screenHeight(450), m_isInitialized(false)
    {
        m_camera.position = {0.0f, 10.0f, 10.0f};
        m_camera.target = {0.0f, 0.0f, 0.0f};
        m_camera.up = {0.0f, 1.0f, 0.0f};
        m_camera.fovy = 45.0f;
        m_camera.projection = CAMERA_PERSPECTIVE;
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

        BeginRenderFrame();

        RenderEntities(scene);

        DrawText(TextFormat("Scene: %s", scene.GetName().c_str()),
                 10, 30, 20, DARKGRAY);

        DrawText(TextFormat("Entities: %d", scene.GetEntityCount()),
                 10, 50, 20, DARKGRAY);

        EndRenderFrame();
    }

    void Renderer::SetCameraPosition(float x, float y, float z)
    {
        m_camera.position = {x, y, z};
    }

    void Renderer::SetCameraTarget(float x, float y, float z)
    {
        m_camera.target = {x, y, z};
    }

    void Renderer::BeginRenderFrame()
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);
    }

    void Renderer::EndRenderFrame()
    {
        DrawFPS(10, 10);

        EndDrawing();
    }

    void Renderer::RenderEntities(const Scene& scene)
    {
        auto entities = scene.GetAllEntities();

        BeginMode3D(m_camera);

        DrawGrid(100, 1.0f);

        for (const auto& entity : entities)
        {
            if (entity->IsActive())
            {
                auto transform = entity->GetTransform();
                if (transform)
                {
                    Vector3 position = transform->GetPosition();

                    // For now, just draw a cube at the entity's position
                    // Later we'll implement proper model rendering
                    DrawCube(position, 1.0f, 1.0f, 1.0f, RED);
                    DrawCubeWires(position, 1.0f, 1.0f, 1.0f, MAROON);
                }

                // Let the entity render itself
                entity->Render();
            }
        }

        EndMode3D();
    }

} // namespace Wayfinder
