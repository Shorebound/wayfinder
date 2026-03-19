#include "Game.h"
#include "EngineConfig.h"
#include "EngineContext.h"
#include "GameplayTag.h"
#include "GameplayTagRegistry.h"
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

        InitializeSubsystems();
        InitializeTagRegistry();
        InitializeWorld();

        // Guard that tears down subsystems on any early return.  Dismissed on success.
        bool committed = false;
        auto cleanup = [&]
        {
            if (!committed)
            {
                GameSubsystems::Unbind();
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

        // Initialize the game state singleton
        m_world.component<ActiveGameState>();
        m_world.set<ActiveGameState>({});

        // Initialize scene settings singleton
        m_world.component<SceneSettings>();
        m_world.set<SceneSettings>({});

        // Initialize active gameplay tags singleton
        m_world.component<ActiveGameplayTags>();
        m_world.set<ActiveGameplayTags>({});

        if (m_moduleRegistry)
        {
            m_moduleRegistry->ApplyToWorld(m_world);
            BindConditionedSystems();

            // Transition to the initial state if one was declared; otherwise, ensure
            // run conditions are evaluated at least once so conditioned systems
            // respect their predicates even if no state/tag changes ever occur.
            const auto& initialState = m_moduleRegistry->GetInitialState();
            if (!initialState.empty())
            {
                TransitionTo(initialState);
            }
            else
            {
                EvaluateRunConditions();
            }
        }
    }

    void Game::Update(const float deltaTime)
    {
        if (!m_running || !m_initialized)
            return;

        if (m_runConditionsDirty)
        {
            EvaluateRunConditions();
            m_runConditionsDirty = false;
        }

        m_world.progress(deltaTime);
    }

    void Game::Shutdown()
    {
        WAYFINDER_INFO(LogGame, "Shutting down game");

        UnloadCurrentScene();

        GameSubsystems::Unbind();
        m_subsystems.Shutdown();

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

    void Game::TransitionTo(const std::string& stateName)
    {
        // Build a name -> descriptor map for O(1) lookups
        const ModuleRegistry::StateDescriptor* targetDesc = nullptr;
        if (m_moduleRegistry)
        {
            for (const auto& desc : m_moduleRegistry->GetStateDescriptors())
            {
                if (desc.Name == stateName)
                {
                    targetDesc = &desc;
                    break;
                }
            }

            if (!targetDesc)
            {
                WAYFINDER_WARNING(LogGame, "TransitionTo: '{}' is not a registered state", stateName);
                return;
            }
        }

        const auto internedName = InternedString::Intern(stateName);
        ActiveGameState& state = m_world.get_mut<ActiveGameState>();

        if (state.Current == internedName)
            return;

        const InternedString oldState = state.Current;

        // Call OnExit for the outgoing state
        if (m_moduleRegistry && !oldState.IsEmpty())
        {
            for (const auto& desc : m_moduleRegistry->GetStateDescriptors())
            {
                if (desc.Name == oldState.GetString() && desc.OnExit)
                {
                    desc.OnExit(m_world);
                    break;
                }
            }
        }

        // Update the singleton
        state.Previous = oldState;
        state.Current = internedName;

        // Call OnEnter for the incoming state
        if (targetDesc && targetDesc->OnEnter)
            targetDesc->OnEnter(m_world);

        WAYFINDER_INFO(LogGame, "State transition: '{}' -> '{}'", oldState.GetString(), stateName);

        m_runConditionsDirty = true;
    }

    std::string_view Game::GetCurrentState() const
    {
        const ActiveGameState& state = m_world.get<ActiveGameState>();
        return state.Current.GetString();
    }

    void Game::AddGameplayTag(const GameplayTag& tag)
    {
        ActiveGameplayTags& tags = m_world.get_mut<ActiveGameplayTags>();
        tags.Tags.AddTag(tag);
        WAYFINDER_INFO(LogGame, "Added gameplay tag: '{}'", tag.GetName());
        m_runConditionsDirty = true;
    }

    void Game::RemoveGameplayTag(const GameplayTag& tag)
    {
        ActiveGameplayTags& tags = m_world.get_mut<ActiveGameplayTags>();
        tags.Tags.RemoveTag(tag);
        WAYFINDER_INFO(LogGame, "Removed gameplay tag: '{}'", tag.GetName());
        m_runConditionsDirty = true;
    }

    bool Game::HasGameplayTag(const GameplayTag& tag) const
    {
        const ActiveGameplayTags& tags = m_world.get<ActiveGameplayTags>();
        return tags.Tags.HasTag(tag);
    }

    void Game::InitializeSubsystems()
    {
        // Register core engine subsystems
        m_subsystems.Register<GameplayTagRegistry>();

        // Register game-module subsystems
        if (m_moduleRegistry)
        {
            for (const auto& [type, factory, predicate] : m_moduleRegistry->GetSubsystemFactories())
                m_subsystems.Register(type, factory, predicate);
        }

        m_subsystems.Initialise();
        GameSubsystems::Bind(&m_subsystems);
    }

    void Game::InitializeTagRegistry()
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

    void Game::BindConditionedSystems()
    {
        if (!m_moduleRegistry)
            return;

        // NOTE: This relies on the flecs system name matching the descriptor name.
        // ModuleRegistry::RegisterSystem callers must use the same name string for
        // both the descriptor and the flecs system() call so lookup succeeds.
        for (const auto& desc : m_moduleRegistry->GetSystems())
        {
            if (!desc.Condition)
                continue;

            flecs::entity sys = m_world.lookup(desc.Name.c_str());
            if (!sys.is_valid())
            {
                WAYFINDER_WARNING(LogGame,
                    "Conditioned system '{}' not found in world. "
                    "Ensure the flecs system name matches the descriptor name.",
                    desc.Name);
                continue;
            }

            m_conditionedSystems.push_back({sys, desc.Condition, true});
        }
    }

    void Game::EvaluateRunConditions()
    {
        for (auto& cs : m_conditionedSystems)
        {
            const bool shouldRun = cs.Condition(m_world);
            if (shouldRun != cs.Enabled)
            {
                if (shouldRun)
                    cs.SystemEntity.enable();
                else
                    cs.SystemEntity.disable();
                cs.Enabled = shouldRun;
            }
        }
    }

} // namespace Wayfinder
