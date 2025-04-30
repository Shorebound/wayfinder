#include "Game.h"
#include "../platform/Input.h"
#include "../scene/Scene.h"
#include "Log.h"
#include "ServiceLocator.h"

namespace Wayfinder
{
    Game::Game() : m_running(false), m_initialized(false)
    {
    }

    Game::~Game()
    {
        if (m_initialized)
        {
            Shutdown();
        }
    }

    bool Game::Initialize()
    {
        WAYFINDER_INFO(LogGame, "Initializing game");

        m_currentScene = std::make_unique<Scene>("Default Scene");
        m_currentScene->Initialize();

        m_running = true;
        m_initialized = true;
        return true;
    }

    void Game::Update(const float deltaTime)
    {
        if (!m_running || !m_initialized)
            return;

        // Example of using the input abstraction
        // auto& input = ServiceLocator::GetInput();
        // if (input.IsKeyPressed(KEY_ESCAPE))
        // {
        //     m_isRunning = false;
        //     return;
        // }

        if (m_currentScene)
        {
            m_currentScene->Update(deltaTime);
        }
    }

    void Game::Shutdown()
    {
        WAYFINDER_INFO(LogGame, "Shutting down game");

        UnloadCurrentScene();

        m_running = false;
        m_initialized = false;
    }

    void Game::LoadScene(const std::string& sceneName)
    {
        UnloadCurrentScene();

        m_currentScene = std::make_unique<Scene>(sceneName);
        m_currentScene->Initialize();

        WAYFINDER_INFO(LogGame, "Loaded scene: {0}", sceneName);
    }

    void Game::UnloadCurrentScene()
    {
        if (m_currentScene)
        {
            m_currentScene->Shutdown();
            m_currentScene = nullptr;
        }
    }

} // namespace Wayfinder
