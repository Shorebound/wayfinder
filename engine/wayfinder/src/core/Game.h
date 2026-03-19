#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <flecs.h>

#include "GameplayTag.h"
#include "GameplayTagRegistry.h"
#include "GameState.h"
#include "SceneSettings.h"
#include "Subsystem.h"
#include "wayfinder_exports.h"
#include "scene/RuntimeComponentRegistry.h"

namespace Wayfinder
{
    class AssetService;
    class ModuleRegistry;
    class Scene;
    struct EngineContext;

    class WAYFINDER_API Game
    {
    public:
        Game();
        ~Game();

        bool Initialize(const EngineContext& ctx);
        void Update(float deltaTime);
        void Shutdown();

        void LoadScene(const std::string& scenePath);
        void UnloadCurrentScene();

        /// Transition to a named game state. Calls OnExit for the old state,
        /// updates the ActiveGameState singleton, calls OnEnter for the new
        /// state, and re-evaluates all run conditions.
        void TransitionTo(const std::string& stateName);

        /// Returns the name of the currently active game state.
        std::string_view GetCurrentState() const;

        /// Add a gameplay tag to the world-level active tag set.
        void AddGameplayTag(const GameplayTag& tag);

        /// Remove a gameplay tag from the world-level active tag set.
        void RemoveGameplayTag(const GameplayTag& tag);

        /// Check if a gameplay tag is active at the world level.
        bool HasGameplayTag(const GameplayTag& tag) const;

        /// Access the gameplay tag registry for tag lookups and validation.
        /// @pre Initialize() must have completed; will terminate if subsystem is missing.
        GameplayTagRegistry& GetTagRegistry() { return GameSubsystems::Get<GameplayTagRegistry>(); }
        /// @copydoc GetTagRegistry()
        const GameplayTagRegistry& GetTagRegistry() const { return GameSubsystems::Get<GameplayTagRegistry>(); }

        Scene* GetCurrentScene() { return m_currentScene.get(); }
        const Scene* GetCurrentScene() const { return m_currentScene.get(); }
        std::shared_ptr<AssetService> GetAssetService() const { return m_assetService; }

        flecs::world& GetWorld() { return m_world; }
        const flecs::world& GetWorld() const { return m_world; }

        void SetRunning(bool isRunning) { m_running = isRunning; }
        bool IsRunning() const { return m_running; }

    private:
        struct ConditionedSystem
        {
            flecs::entity SystemEntity;
            RunCondition Condition;
            bool Enabled = true;
        };

        void InitializeWorld();
        void InitializeSubsystems();
        void InitializeTagRegistry();
        void BindConditionedSystems();
        void EvaluateRunConditions();

        flecs::world m_world;
        SubsystemCollection<GameSubsystem> m_subsystems;
        RuntimeComponentRegistry m_componentRegistry;
        std::unique_ptr<Scene> m_currentScene;
        std::shared_ptr<AssetService> m_assetService;
        const ModuleRegistry* m_moduleRegistry = nullptr;
        std::vector<ConditionedSystem> m_conditionedSystems;
        bool m_running = false;
        bool m_initialized = false;
        bool m_runConditionsDirty = false;
    };

} // namespace Wayfinder
