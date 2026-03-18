#include "Game.h"
#include "../assets/AssetService.h"
#include "../platform/Input.h"
#include "../scene/Scene.h"
#include "../scene/entity/Entity.h"
#include "../scene/Components.h"
#include "Log.h"
#include "ServiceLocator.h"

#include <filesystem>

namespace
{
    std::filesystem::path GetExecutableDirectory()
    {
        // std::filesystem::current_path() returns the working directory.
        // For a more robust solution, platform-specific APIs can be used later.
        return std::filesystem::current_path();
    }

    std::filesystem::path ResolveBootScenePath()
    {
        const std::filesystem::path executableDirectory = GetExecutableDirectory();
        const std::filesystem::path workingDirectory = std::filesystem::current_path();

        const std::array<std::filesystem::path, 4> candidates = {
            executableDirectory / "assets/scenes/default_scene.toml",
            workingDirectory / "assets/scenes/default_scene.toml",
            workingDirectory / "sandbox/journey/assets/scenes/default_scene.toml",
            executableDirectory / "../../../sandbox/journey/assets/scenes/default_scene.toml"
        };

        for (const std::filesystem::path& candidate : candidates)
        {
            if (std::filesystem::exists(candidate))
            {
                return std::filesystem::weakly_canonical(candidate);
            }
        }

        return {};
    }
}

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

        m_assetService = std::make_shared<AssetService>();
        m_currentScene = std::make_unique<Scene>("Default Scene");
        m_currentScene->SetAssetService(m_assetService);
        m_currentScene->Initialize();

        const std::filesystem::path bootScenePath = ResolveBootScenePath();
        if (bootScenePath.empty())
        {
            WAYFINDER_ERROR(LogGame, "Could not resolve a bootstrap scene file.");
            return false;
        }

        if (!m_currentScene->LoadFromFile(bootScenePath.string()))
        {
            WAYFINDER_ERROR(LogGame, "Failed to load bootstrap scene: {0}", bootScenePath.string());
            return false;
        }

        WAYFINDER_INFO(LogGame, "Loaded bootstrap scene from: {0}", bootScenePath.string());

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
        m_currentScene->SetAssetService(m_assetService);
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
