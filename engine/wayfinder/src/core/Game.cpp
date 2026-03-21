#include "Game.h"
#include "GameContext.h"
#include "GameplayTag.h"
#include "GameplayTagRegistry.h"
#include "GameStateMachine.h"
#include "InternedString.h"
#include "Log.h"
#include "ModuleRegistry.h"
#include "ProjectDescriptor.h"
#include "SceneSettings.h"
#include "../assets/AssetService.h"
#include "../scene/Scene.h"

#include <filesystem>
#include <functional>
#include <unordered_map>

namespace Wayfinder
{
    Game::Game() = default;

    Game::~Game()
    {
        if (m_initialised)
        {
            Shutdown();
        }
    }

    bool Game::Initialise(const GameContext& ctx)
    {
        WAYFINDER_INFO(LogGame, "Initialising game");

        m_moduleRegistry = ctx.moduleRegistry;
        m_assetService = std::make_shared<AssetService>();

        InitialiseSubsystems();
        InitialiseTagRegistry();
        InitialiseWorld();

        // Guard that tears down subsystems on any early return.  Dismissed on success.
        bool committed = false;
        auto cleanup = [&]
        {
            if (!committed)
            {
                GameSubsystems::Bind(nullptr);
                m_subsystems.Shutdown();
            }
        };
        struct CleanupGuard
        {
            std::function<void()> Fn;
            ~CleanupGuard() { Fn(); }
        } guard{cleanup};

        const auto bootScenePath = ctx.project.ResolveBootScene();

        if (!std::filesystem::exists(bootScenePath))
        {
            WAYFINDER_ERROR(LogGame, "Boot scene not found: {}", bootScenePath.string());
            return false;
        }

        std::error_code canonicalError;
        const auto resolvedPath = std::filesystem::weakly_canonical(bootScenePath, canonicalError);
        if (canonicalError)
        {
            WAYFINDER_ERROR(LogGame,
                            "Failed to resolve canonical path for boot scene '{}': {}",
                            bootScenePath.string(),
                            canonicalError.message());
            return false;
        }

        m_currentScene = std::make_unique<Scene>(m_world, m_componentRegistry, "Default Scene");
        m_currentScene->SetAssetService(m_assetService);

        if (!m_currentScene->LoadFromFile(resolvedPath.string()))
        {
            WAYFINDER_ERROR(LogGame, "Failed to load bootstrap scene: {}", resolvedPath.string());
            m_currentScene.reset();
            return false;
        }

        WAYFINDER_INFO(LogGame, "Loaded bootstrap scene from: {}", resolvedPath.string());

        committed = true;
        m_running = true;
        m_initialised = true;
        return true;
    }

    void Game::InitialiseWorld()
    {
        // Build the unified component registry: core entries + game entries
        m_componentRegistry.AddCoreEntries();
        if (m_moduleRegistry)
            m_componentRegistry.AddGameEntries(*m_moduleRegistry);

        // Register ECS infrastructure and all components into the world
        Scene::RegisterCoreECS(m_world);
        m_componentRegistry.RegisterComponents(m_world);

        // Initialise the game state singleton
        m_world.component<ActiveGameState>();
        m_world.set<ActiveGameState>({});

        // Initialise scene settings singleton
        m_world.component<SceneSettings>();
        m_world.set<SceneSettings>({});

        // Initialise active gameplay tags singleton
        m_world.component<ActiveGameplayTags>();
        m_world.set<ActiveGameplayTags>({});

        if (m_moduleRegistry)
        {
            m_moduleRegistry->ApplyToWorld(m_world);
        }

        // Configure and set up the state machine after world registration
        m_stateMachine = GameSubsystems::Find<GameStateMachine>();
        if (m_stateMachine)
        {
            m_stateMachine->Configure(m_world, m_moduleRegistry);
            m_stateMachine->Setup();
        }
    }

    void Game::Update(const float deltaTime)
    {
        if (!m_running || !m_initialised)
            return;

        if (m_stateMachine)
            m_stateMachine->Update();

        m_world.progress(deltaTime);
    }

    void Game::Shutdown()
    {
        if (!m_initialised)
            return;

        WAYFINDER_INFO(LogGame, "Shutting down game");

        UnloadCurrentScene();

        GameSubsystems::Bind(nullptr);
        m_subsystems.Shutdown();

        m_stateMachine = nullptr;
        m_running = false;
        m_initialised = false;
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

    void Game::TransitionTo(const std::string& stateName)
    {
        if (!m_stateMachine)
        {
            WAYFINDER_WARNING(LogGame, "TransitionTo('{}') called but no state machine is configured", stateName);
            return;
        }
        m_stateMachine->TransitionTo(stateName);
    }

    std::string_view Game::GetCurrentState() const
    {
        if (m_stateMachine)
            return m_stateMachine->GetCurrentState();
        return {};
    }

    void Game::AddGameplayTag(const GameplayTag& tag)
    {
        ActiveGameplayTags& tags = m_world.get_mut<ActiveGameplayTags>();
        tags.Tags.AddTag(tag);
        WAYFINDER_INFO(LogGame, "Added gameplay tag: '{}'", tag.GetName());
        if (m_stateMachine)
            m_stateMachine->MarkDirty();
    }

    void Game::RemoveGameplayTag(const GameplayTag& tag)
    {
        ActiveGameplayTags& tags = m_world.get_mut<ActiveGameplayTags>();
        tags.Tags.RemoveTag(tag);
        WAYFINDER_INFO(LogGame, "Removed gameplay tag: '{}'", tag.GetName());
        if (m_stateMachine)
            m_stateMachine->MarkDirty();
    }

    bool Game::HasGameplayTag(const GameplayTag& tag) const
    {
        const ActiveGameplayTags& tags = m_world.get<ActiveGameplayTags>();
        return tags.Tags.HasTag(tag);
    }

    void Game::InitialiseSubsystems()
    {
        // Register core engine subsystems
        m_subsystems.Register<GameplayTagRegistry>();
        m_subsystems.Register<GameStateMachine>();

        // Register game-module subsystems
        if (m_moduleRegistry)
        {
            for (const auto& [type, factory, predicate] : m_moduleRegistry->GetSubsystemFactories())
                m_subsystems.Register(type, factory, predicate);
        }

        m_subsystems.Initialise();
        GameSubsystems::Bind(&m_subsystems);
    }

    void Game::InitialiseTagRegistry()
    {
        auto& tagRegistry = GameSubsystems::Get<GameplayTagRegistry>();

        if (!m_moduleRegistry)
            return;

        // Load tag files registered by plugins (paths relative to config dir)
        const auto& project = m_moduleRegistry->GetProject();
        const auto configDir = project.ResolveConfigDir();
        for (const auto& relPath : m_moduleRegistry->GetTagFiles())
        {
            const auto fullPath = configDir / relPath;
            if (std::filesystem::exists(fullPath))
                tagRegistry.LoadTagFile(fullPath);
            else
                WAYFINDER_WARNING(LogGame, "Tag file not found: '{}'", fullPath.string());
        }

        // Register code-defined tags
        for (const auto& desc : m_moduleRegistry->GetRegisteredTags())
            tagRegistry.RegisterTag(desc.Name, desc.Comment);
    }

} // namespace Wayfinder
