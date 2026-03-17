#include "Renderer.h"
#include "RenderFrame.h"
#include "RenderPipeline.h"
#include "RenderResources.h"

#include "../core/ServiceLocator.h"
#include "../rendering/RenderAPI.h"
#include "../rendering/GraphicsContext.h"

namespace Wayfinder
{
    Renderer::Renderer()
        : m_screenWidth(800), m_screenHeight(450), m_isInitialized(false)
    {
        // Initialize camera with default values
        m_camera.Position = {0.0f, 10.0f, 10.0f};
        m_camera.Target = {0.0f, 0.0f, 0.0f};
        m_camera.Up = {0.0f, 1.0f, 0.0f};
        m_camera.FOV = 45.0f;
        m_camera.ProjectionType = 0; // CAMERA_PERSPECTIVE
        m_clearColor = Color::White();
        m_renderPipeline = std::make_unique<RenderPipeline>();
        m_renderResources = std::make_unique<RenderResourceCache>();
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
        m_renderPipeline = std::make_unique<RenderPipeline>();
        m_renderResources = std::make_unique<RenderResourceCache>();
        m_isInitialized = false;
    }

    void Renderer::Render(const RenderFrame& frame)
    {
        if (!m_isInitialized)
            return;

        if (!frame.Views.empty())
        {
            m_camera = frame.Views.front().CameraState;
            m_clearColor = frame.Views.front().ClearColor;
        }

        BeginFrame();

        if (m_renderResources)
        {
            m_renderResources->SetAssetRoot(frame.AssetRoot);
        }

        if (m_renderPipeline && m_renderResources)
        {
            m_renderPipeline->Execute(frame, ServiceLocator::GetRenderAPI(), *m_renderResources);
        }

        // Get render API from service locator
        auto& renderAPI = ServiceLocator::GetRenderAPI();

        // Draw scene info
        renderAPI.DrawText("Scene: " + frame.SceneName, 10, 30, 20, Color::DarkGray());

        EndFrame();
    }

    void Renderer::SetCameraPosition(float x, float y, float z)
    {
        m_camera.Position = {x, y, z};
    }

    void Renderer::SetCameraTarget(float x, float y, float z)
    {
        m_camera.Target = {x, y, z};
    }

    void Renderer::BeginFrame()
    {
        auto& graphicsContext = ServiceLocator::GetGraphicsContext();
        //auto& renderAPI = ServiceLocator::GetRenderAPI();

        graphicsContext.BeginFrame();
        graphicsContext.Clear(
            static_cast<float>(m_clearColor.r) / 255.0f,
            static_cast<float>(m_clearColor.g) / 255.0f,
            static_cast<float>(m_clearColor.b) / 255.0f,
            static_cast<float>(m_clearColor.a) / 255.0f);
    }

    void Renderer::EndFrame()
    {
        auto& renderAPI = ServiceLocator::GetRenderAPI();
        auto& graphicsContext = ServiceLocator::GetGraphicsContext();

        renderAPI.DrawFPS(10, 10);
        graphicsContext.EndFrame();
    }

} // namespace Wayfinder
