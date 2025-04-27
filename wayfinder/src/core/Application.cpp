#include "Application.h"
#include "Game.h"
#include "../rendering/Renderer.h"
#include "../scene/Scene.h"

namespace Wayfinder
{

    Application::Application(const Config &config) : m_isRunning(false), m_lastFrameTime(0.0)
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
        InitWindow(m_config.screenWidth, m_config.screenHeight, m_config.windowTitle.c_str());
        SetExitKey(0);

        // SetTargetFPS(60);

        m_game = std::make_unique<Game>();
        m_renderer = std::make_unique<Renderer>();

        if (!m_game->Initialize())
        {
            TraceLog(LOG_ERROR, "Failed to initialize Game");
            return false;
        }

        if (!m_renderer->Initialize(m_config.screenWidth, m_config.screenHeight))
        {
            TraceLog(LOG_ERROR, "Failed to initialize Renderer");
            return false;
        }

        m_lastFrameTime = GetFrameTime();
        m_isRunning = true;
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
        while (m_isRunning && !WindowShouldClose())
        {
            float deltaTime = GetFrameTime();
            if (m_game)
            {
                m_game->Update(deltaTime);
                if (m_renderer)
                {
                    const auto *currentScene = m_game->GetCurrentScene();
                    if (currentScene)
                    {
                        m_renderer->Render(*currentScene);
                    }
                }
            }

            m_isRunning = m_game->IsRunning();
        }
    }

    void Application::Loop(Application *app)
    {
        app->Loop();
    }

    void Application::Shutdown()
    {
        if (m_renderer)
        {
            m_renderer->Shutdown();
            m_renderer = nullptr;
        }

        if (m_game)
        {
            m_game->Shutdown();
            m_game = nullptr;
        }

        if (IsWindowReady())
        {
            CloseWindow();
        }
    }

} // namespace Wayfinder
