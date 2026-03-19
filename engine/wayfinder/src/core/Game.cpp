#include "Game.h"
#include "EngineConfig.h"
#include "EngineContext.h"
#include "Log.h"
#include "ModuleRegistry.h"
#include "ProjectDescriptor.h"
#include "../assets/AssetService.h"
#include "../scene/Scene.h"

#include <filesystem>

namespace Wayfinder
{
    Game::Game() = default;

    Game::~Game()
    {
        if (m_initialized)
        {
            Shutdown();
        }
    }

    bool Game::Initialize(const EngineContext& ctx)
    {
        WAYFINDER_INFO(LogGame, "Initializing game");

        m_moduleRegistry = ctx.moduleRegistry;
        m_assetService = std::make_shared<AssetService>();

        InitializeWorld();

        const auto bootScenePath = ctx.project.ResolveBootScene();

        if (!std::filesystem::exists(bootScenePath))
        {
            WAYFINDER_ERROR(LogGame, "Boot scene not found: {}", bootScenePath.string());
            return false;
        }

        const auto resolvedPath = std::filesystem::weakly_canonical(bootScenePath);

        m_currentScene = std::make_unique<Scene>(m_world, m_componentRegistry, "Default Scene");
        m_currentScene->SetAssetService(m_assetService);

        if (!m_currentScene->LoadFromFile(resolvedPath.string()))
        {
            WAYFINDER_ERROR(LogGame, "Failed to load bootstrap scene: {}", resolvedPath.string());
            return false;
        }

        WAYFINDER_INFO(LogGame, "Loaded bootstrap scene from: {}", resolvedPath.string());

        m_running = true;
        m_initialized = true;
        return true;
    }

    void Game::InitializeWorld()
    {
        // Build the unified component registry: core entries + game entries
        m_componentRegistry.AddCoreEntries();
        if (m_moduleRegistry)
            m_componentRegistry.AddGameEntries(*m_moduleRegistry);

        // Register ECS infrastructure and all components into the world
        Scene::RegisterCoreECS(m_world);
        m_componentRegistry.RegisterComponents(m_world);

        if (m_moduleRegistry)
            m_moduleRegistry->ApplyToWorld(m_world);
    }

    void Game::Update(const float deltaTime)
    {
        if (!m_running || !m_initialized)
            return;

        m_world.progress(deltaTime);
    }

    void Game::Shutdown()
    {
        WAYFINDER_INFO(LogGame, "Shutting down game");

        UnloadCurrentScene();

        m_running = false;
        m_initialized = false;
    }

    void Game::LoadScene(const std::string& scenePath)
    {
        UnloadCurrentScene();

        m_currentScene = std::make_unique<Scene>(m_world, m_componentRegistry, scenePath);
        m_currentScene->SetAssetService(m_assetService);

        if (std::filesystem::exists(scenePath))
        {
            m_currentScene->LoadFromFile(scenePath);
        }

        WAYFINDER_INFO(LogGame, "Loaded scene: {}", scenePath);
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
