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

        // Initialize the game state singleton
        m_world.component<ActiveGameState>();
        m_world.set<ActiveGameState>({});

        if (m_moduleRegistry)
        {
            m_moduleRegistry->ApplyToWorld(m_world);
            BindConditionedSystems();

            // Transition to the initial state if one was declared
            const auto& initialState = m_moduleRegistry->GetInitialState();
            if (!initialState.empty())
                TransitionTo(initialState);
        }
    }

    void Game::Update(const float deltaTime)
    {
        if (!m_running || !m_initialized)
            return;

        EvaluateRunConditions();
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

    void Game::TransitionTo(const std::string& stateName)
    {
        ActiveGameState& state = m_world.get_mut<ActiveGameState>();

        if (state.Current == stateName)
            return;

        const std::string oldState = state.Current;

        // Call OnExit for the outgoing state
        if (m_moduleRegistry && !oldState.empty())
        {
            for (const auto& desc : m_moduleRegistry->GetStateDescriptors())
            {
                if (desc.Name == oldState && desc.OnExit)
                    desc.OnExit(m_world);
            }
        }

        // Update the singleton
        state.Previous = oldState;
        state.Current = stateName;

        // Call OnEnter for the incoming state
        if (m_moduleRegistry && !stateName.empty())
        {
            for (const auto& desc : m_moduleRegistry->GetStateDescriptors())
            {
                if (desc.Name == stateName && desc.OnEnter)
                    desc.OnEnter(m_world);
            }
        }

        WAYFINDER_INFO(LogGame, "State transition: '{}' -> '{}'", oldState, stateName);

        EvaluateRunConditions();
    }

    std::string_view Game::GetCurrentState() const
    {
        const ActiveGameState& state = m_world.get<ActiveGameState>();
        return state.Current;
    }

    void Game::BindConditionedSystems()
    {
        if (!m_moduleRegistry)
            return;

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
