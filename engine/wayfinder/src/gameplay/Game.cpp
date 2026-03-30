#include "Game.h"
#include "GameContext.h"
#include "GameStateMachine.h"
#include "GameplayTag.h"
#include "GameplayTagRegistry.h"
#include "assets/AssetService.h"
#include "core/InternedString.h"
#include "core/Log.h"
#include "core/Result.h"
#include "plugins/PluginRegistry.h"
#include "project/ProjectDescriptor.h"
#include "scene/Scene.h"
#include "scene/SceneSettings.h"

#include <filesystem>
#include <string_view>
#include <unordered_map>

namespace Wayfinder
{
    Game::Game(const Plugins::PluginRegistry& pluginRegistry) : m_pluginRegistry(pluginRegistry) {}

    Game::~Game()
    {
        if (m_initialised)
        {
            Shutdown();
        }
    }

    Result<void> Game::Initialise(const GameContext& ctx)
    {
        Log::Info(LogGame, "Initialising game");

        if (&ctx.pluginRegistry != &m_pluginRegistry)
        {
            Log::Error(LogGame, "GameContext::pluginRegistry must refer to the same PluginRegistry instance passed to Game's constructor");
            return MakeError("GameContext plugin registry does not match Game constructor");
        }

        m_assetService = std::make_shared<AssetService>();

        InitialiseSubsystems();
        InitialiseTagRegistry();
        InitialiseWorld();

        // Guard that tears down subsystems on any early return.  Dismissed on success.
        struct CleanupState
        {
            bool Committed = false;
            Game* Self = nullptr;
        };
        CleanupState cleanupState{.Self = this};
        struct CleanupGuard
        {
            CleanupState* State = nullptr;
            ~CleanupGuard() noexcept
            {
                if (!State->Committed)
                {
                    GameSubsystems::Unbind();
                    State->Self->m_subsystems.Shutdown();
                    State->Self->m_stateMachine = nullptr;
                }
            }
        } const guard{.State = &cleanupState};

        const auto bootScenePath = ctx.project.ResolveBootScene();

        if (!std::filesystem::exists(bootScenePath))
        {
            Log::Error(LogGame, "Boot scene not found: {}", bootScenePath.string());
            return MakeError("Boot scene not found");
        }

        std::error_code canonicalError;
        const auto resolvedPath = std::filesystem::weakly_canonical(bootScenePath, canonicalError);
        if (canonicalError)
        {
            Log::Error(LogGame, "Failed to resolve canonical path for boot scene '{}': {}", bootScenePath.string(), canonicalError.message());
            return MakeError("Failed to resolve boot scene path");
        }

        m_currentScene = std::make_unique<Scene>(m_world, m_componentRegistry, "Default Scene");
        m_currentScene->SetAssetService(m_assetService);

        if (auto loadResult = m_currentScene->LoadFromFile(resolvedPath.string()); !loadResult)
        {
            Log::Error(LogGame, "Failed to load bootstrap scene: {}", resolvedPath.string());
            m_currentScene.reset();
            return std::unexpected(loadResult.error());
        }

        Log::Info(LogGame, "Loaded bootstrap scene from: {}", resolvedPath.string());

        cleanupState.Committed = true;
        m_running = true;
        m_initialised = true;
        return {};
    }

    void Game::InitialiseWorld()
    {
        // Build the unified component registry: core entries + game entries
        m_componentRegistry.AddCoreEntries();
        m_componentRegistry.AddGameEntries(m_pluginRegistry);

        // Register ECS infrastructure and all components into the world
        Scene::RegisterCoreComponents(m_world);
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

        m_pluginRegistry.ApplyToWorld(m_world);

        // Configure and set up the state machine after world registration
        m_stateMachine = GameSubsystems::Find<GameStateMachine>();
        if (m_stateMachine)
        {
            m_stateMachine->Configure(m_world, m_pluginRegistry);
            m_stateMachine->Setup();
        }
    }

    void Game::Update(const float deltaTime)
    {
        if (!m_running || !m_initialised)
        {
            return;
        }

        if (m_stateMachine)
        {
            m_stateMachine->Update();
        }

        m_world.progress(deltaTime);
    }

    void Game::Shutdown()
    {
        if (!m_initialised)
        {
            return;
        }

        Log::Info(LogGame, "Shutting down game");

        UnloadCurrentScene();

        GameSubsystems::Unbind();
        m_subsystems.Shutdown();

        m_stateMachine = nullptr;
        m_running = false;
        m_initialised = false;
    }

    void Game::LoadScene(const std::string_view scenePath)
    {
        const std::string pathStr(scenePath);
        if (!std::filesystem::exists(pathStr))
        {
            Log::Warn(LogGame, "Scene file not found: {}", pathStr);
            return;
        }

        auto newScene = std::make_unique<Scene>(m_world, m_componentRegistry, pathStr);
        newScene->SetAssetService(m_assetService);

        if (auto result = newScene->LoadFromFile(pathStr); !result)
        {
            Log::Error(LogGame, "Failed to load scene '{}': {}", pathStr, result.error().GetMessage());
            return;
        }

        UnloadCurrentScene();
        m_currentScene = std::move(newScene);
        Log::Info(LogGame, "Loaded scene: {}", pathStr);
    }

    void Game::UnloadCurrentScene()
    {
        if (m_currentScene)
        {
            m_currentScene->Shutdown();
            m_currentScene = nullptr;
        }
    }

    void Game::TransitionTo(const std::string_view stateName)
    {
        if (!m_stateMachine)
        {
            Log::Warn(LogGame, "TransitionTo('{}') called but no state machine is configured", stateName);
            return;
        }
        m_stateMachine->TransitionTo(stateName);
    }

    std::string_view Game::GetCurrentState() const
    {
        if (m_stateMachine)
        {
            return m_stateMachine->GetCurrentState();
        }
        return {};
    }

    void Game::AddGameplayTag(const GameplayTag& tag)
    {
        auto& tags = m_world.get_mut<ActiveGameplayTags>();
        tags.Tags.AddTag(tag);
        Log::Info(LogGame, "Added gameplay tag: '{}'", tag.GetName());
        if (m_stateMachine)
        {
            m_stateMachine->MarkDirty();
        }
    }

    void Game::RemoveGameplayTag(const GameplayTag& tag)
    {
        auto& tags = m_world.get_mut<ActiveGameplayTags>();
        tags.Tags.RemoveTag(tag);
        Log::Info(LogGame, "Removed gameplay tag: '{}'", tag.GetName());
        if (m_stateMachine)
        {
            m_stateMachine->MarkDirty();
        }
    }

    bool Game::HasGameplayTag(const GameplayTag& tag) const
    {
        const auto& tags = m_world.get<ActiveGameplayTags>();
        return tags.Tags.HasTag(tag);
    }

    void Game::InitialiseSubsystems()
    {
        // Register core engine subsystems
        m_subsystems.Register<GameplayTagRegistry>();
        m_subsystems.Register<GameStateMachine>();

        // Register plugin subsystems
        for (const auto& [type, factory, predicate] : m_pluginRegistry.GetSubsystemFactories())
        {
            m_subsystems.Register(type, factory, predicate);
        }

        m_subsystems.Initialise();
        GameSubsystems::Bind(&m_subsystems);
    }

    void Game::InitialiseTagRegistry()
    {
        auto& tagRegistry = GameSubsystems::Get<GameplayTagRegistry>();

        // Load tag files registered by plugins (paths relative to config dir)
        const auto& project = m_pluginRegistry.GetProject();
        const auto configDir = project.ResolveConfigDir();
        for (const auto& relPath : m_pluginRegistry.GetTagFiles())
        {
            const auto fullPath = configDir / relPath;
            if (std::filesystem::exists(fullPath))
            {
                tagRegistry.LoadTagFile(fullPath);
            }
            else
            {
                Log::Warn(LogGame, "Tag file not found: '{}'", fullPath.string());
            }
        }

        // Register code-defined tags
        for (const auto& desc : m_pluginRegistry.GetRegisteredTags())
        {
            tagRegistry.RegisterTag(desc.Name, desc.Comment);
        }
    }

} // namespace Wayfinder
