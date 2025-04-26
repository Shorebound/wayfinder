#include "../include/Game.h"
#include "../include/Scene.h"
#include "raylib.h"

namespace Wayfinder {

Game::Game()
    : m_isRunning(false)
    , m_isInitialized(false)
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
    
    // Create a default scene
    m_currentScene = std::make_shared<Scene>("Default Scene");
    m_currentScene->Initialize();
    
    m_isRunning = true;
    m_isInitialized = true;
    return true;
}

void Game::Update(float deltaTime)
{
    if (!m_isRunning || !m_isInitialized)
        return;
    
    // Check for exit condition (ESC key)
    if (IsKeyPressed(KEY_ESCAPE))
    {
        m_isRunning = false;
        return;
    }
    
    // Update the current scene
    if (m_currentScene)
    {
        m_currentScene->Update(deltaTime);
    }
    
    // Game-specific logic can go here
}

void Game::Shutdown()
{
    TraceLog(LOG_INFO, "Shutting down game");
    
    // Unload the current scene
    UnloadCurrentScene();
    
    m_isRunning = false;
    m_isInitialized = false;
}

void Game::LoadScene(const std::string& sceneName)
{
    // Unload current scene if it exists
    UnloadCurrentScene();
    
    // Create and initialize the new scene
    m_currentScene = std::make_shared<Scene>(sceneName);
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
