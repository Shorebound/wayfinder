#include "Game.h"
#include "../scene/Scene.h"
#include "raylib.h"

namespace Wayfinder
{

    Game::Game() : m_isRunning(false), m_isInitialized(false)
    {
    }

    Game::~Game()
    {
        if (m_isInitialized)
        {
            Shutdown();
        }
    }

    bool Game::Initialize()
    {
        TraceLog(LOG_INFO, "Initializing game");

        m_currentScene = std::make_unique<Scene>("Default Scene");
        m_currentScene->Initialize();

        m_isRunning = true;
        m_isInitialized = true;
        return true;
    }

    void Game::Update(float deltaTime)
    {
        if (!m_isRunning || !m_isInitialized)
            return;

        /*if (IsKeyPressed(KEY_ESCAPE))
        {
            m_isRunning = false;
            return;
        }*/

        if (m_currentScene)
        {
            m_currentScene->Update(deltaTime);
        }
    }

    void Game::Shutdown()
    {
        TraceLog(LOG_INFO, "Shutting down game");

        UnloadCurrentScene();

        m_isRunning = false;
        m_isInitialized = false;
    }

    void Game::LoadScene(const std::string& sceneName)
    {
        UnloadCurrentScene();

        m_currentScene = std::make_unique<Scene>(sceneName);
        m_currentScene->Initialize();

        TraceLog(LOG_INFO, "Loaded scene: %s", sceneName.c_str());
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
