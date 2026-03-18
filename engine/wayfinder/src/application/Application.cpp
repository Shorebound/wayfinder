#include "Application.h"
#include "../core/Game.h"
#include "../core/Log.h"
#include "../core/ServiceLocator.h"
#include "../platform/Window.h"
#include "../platform/Time.h"
#include "../rendering/RenderDevice.h"
#include "../rendering/Renderer.h"
#include "../rendering/SceneRenderExtractor.h"
#include "../scene/Scene.h"

namespace Wayfinder
{
    Application::Application(const Config& config) : m_running(false)
    {
        m_config = config;
        Initialize();
    }

    Application::~Application()
    {
        Shutdown();
    }

    bool Application::Initialize()
    {
        Log::Init();
        WAYFINDER_INFO(LogEngine, "Initializing Wayfinder Engine");

        // Platform services (input, time)
        ServiceLocator::Initialize(m_config.Backends);

        // Window — must be created before RenderDevice
        const auto windowConfig = Window::Config{
            m_config.ScreenWidth,
            m_config.ScreenHeight,
            m_config.WindowTitle,
            m_config.VSync};

        m_window = Window::Create(windowConfig, m_config.Backends.Platform);

        if (!m_window->Initialize())
        {
            WAYFINDER_ERROR(LogEngine, "Failed to initialize Window");
            return false;
        }

        // GPU device — needs window handle for swapchain
        m_device = RenderDevice::Create(m_config.Backends.Rendering);

        if (!m_device || !m_device->Initialize(*m_window))
        {
            WAYFINDER_ERROR(LogEngine, "Failed to initialize RenderDevice");
            return false;
        }

        // Game and renderer
        m_game = std::make_unique<Game>();
        m_renderer = std::make_unique<Renderer>();
        m_sceneRenderExtractor = std::make_unique<SceneRenderExtractor>();

        if (!m_game->Initialize())
        {
            WAYFINDER_ERROR(LogEngine, "Failed to initialize Game");
            return false;
        }

        m_renderer->SetAssetService(m_game->GetAssetService());

        if (!m_renderer->Initialize(*m_device, m_config.ScreenWidth, m_config.ScreenHeight))
        {
            WAYFINDER_ERROR(LogEngine, "Failed to initialize Renderer");
            return false;
        }

        m_running = true;
        return true;
    }

    void Application::Run()
    {
        Loop();
    }

    void Application::Loop()
    {
        auto& time = ServiceLocator::GetTime();

        while (m_running && !m_window->ShouldClose())
        {
            time.Update();
            m_window->Update();

            if (m_game)
            {
                m_game->Update(time.GetDeltaTime());

                if (m_renderer)
                {
                    if (const auto* currentScene = m_game->GetCurrentScene())
                    {
                        m_renderer->Render(m_sceneRenderExtractor->Extract(*currentScene));
                    }
                }
            }

            m_running = m_game->IsRunning();
        }
    }

    void Application::Loop(Application* app)
    {
        app->Loop();
    }

    void Application::Shutdown()
    {
        WAYFINDER_INFO(LogEngine, "Shutting down Wayfinder Engine");

        if (m_renderer)
        {
            m_renderer->Shutdown();
            m_renderer = nullptr;
        }

        m_sceneRenderExtractor = nullptr;

        if (m_game)
        {
            m_game->Shutdown();
            m_game = nullptr;
        }

        if (m_device)
        {
            m_device->Shutdown();
            m_device = nullptr;
        }

        if (m_window)
        {
            m_window->Shutdown();
            m_window = nullptr;
        }

        ServiceLocator::Shutdown();
        Log::Shutdown();
    }
} // namespace Wayfinder

