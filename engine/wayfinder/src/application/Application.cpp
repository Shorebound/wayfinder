#include "Application.h"
#include "../core/Game.h"
#include "../core/Log.h"
#include "../core/ServiceLocator.h"
#include "../platform/Window.h"
#include "../platform/Time.h"
#include "../rendering/Renderer.h"
#include "../rendering/SceneRenderExtractor.h"
#include "../rendering/GraphicsContext.h"
#include "../rendering/RenderAPI.h"
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
        // Initialize logging first
        Log::Init();
        WAYFINDER_INFO(LogEngine, "Initializing Wayfinder Engine");

        // Initialize the service locator with platform services
        ServiceLocator::Initialize(m_config.Backends);

        // Create and initialize window
        //m_window = ServiceLocator::GetWindow();

        // Create game and renderer
        const auto windowConfig = Window::Config{
            m_config.ScreenWidth,
            m_config.ScreenHeight,
            m_config.WindowTitle,
            m_config.VSync};
        
        m_window = Window::Create(windowConfig, m_config.Backends.Platform);
        m_game = std::make_unique<Game>();
        m_renderer = std::make_unique<Renderer>();
        m_sceneRenderExtractor = std::make_unique<SceneRenderExtractor>();

        if (!m_window->Initialize())
        {
            WAYFINDER_ERROR(LogEngine, "Failed to initialize Window");
            return false;
        }

        if (!m_game->Initialize())
        {
            WAYFINDER_ERROR(LogEngine, "Failed to initialize Game");
            return false;
        }

        m_renderer->SetAssetService(m_game->GetAssetService());

        if (!m_renderer->Initialize(m_config.ScreenWidth, m_config.ScreenHeight))
        {
            WAYFINDER_ERROR(LogEngine, "Failed to initialize Renderer");
            return false;
        }

        m_running = true;
        return true;
    }

    void Application::Run()
    {
#if defined(PLATFORM_WEB)
        emscripten_set_main_loop_arg(Loop, this, 0, 1);
#else
        Loop();
#endif
    }

    void Application::Loop()
    {
        auto& time = ServiceLocator::GetTime();

        while (m_running && !m_window->ShouldClose())
        {
            // Update time
            time.Update();
            float deltaTime = time.GetDeltaTime();

            WAYFINDER_INFO(LogRenderer, "Frame time: {0}", deltaTime);

            // Update window
            m_window->Update();

            // Update game
            if (m_game)
            {
                m_game->Update(deltaTime);

                // Render current scene
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

        if (m_window)
        {
            m_window->Shutdown();
            m_window = nullptr;
        }

        // Shutdown service locator (which will shutdown window, etc.)
        ServiceLocator::Shutdown();

        Log::Shutdown();
    }
} // namespace Wayfinder

