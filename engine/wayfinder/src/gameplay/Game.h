#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ecs/Flecs.h"

#include "GameState.h"
#include "GameStateMachine.h"
#include "GameplayTag.h"
#include "GameplayTagRegistry.h"
#include "app/Subsystem.h"
#include "core/Result.h"
#include "scene/RuntimeComponentRegistry.h"
#include "scene/SceneSettings.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    class AssetService;
    class ModuleRegistry;
    class Scene;
    struct GameContext;

    class WAYFINDER_API Game
    {
    public:
        Game();
        ~Game();

        /**
         * @brief Initialise the game: subsystems, ECS world, and boot scene.
         * @param ctx  GameContext providing the project descriptor and optional
         *             module registry.
         * @return A successful Result on success, or an Error if the boot
         *         scene cannot be found, resolved, or loaded.
         */
        Result<void> Initialise(const GameContext& ctx);
        void Update(float deltaTime);
        void Shutdown();

        void LoadScene(const std::string& scenePath);
        void UnloadCurrentScene();

        /// Transition to a named game state. Forwards to GameStateMachine.
        void TransitionTo(const std::string& stateName);

        /// Returns the name of the currently active game state.
        std::string_view GetCurrentState() const;

        /// Add a gameplay tag to the world-level active tag set.
        void AddGameplayTag(const GameplayTag& tag);

        /// Remove a gameplay tag from the world-level active tag set.
        void RemoveGameplayTag(const GameplayTag& tag);

        /// Check if a gameplay tag is active at the world level.
        bool HasGameplayTag(const GameplayTag& tag) const;

        /**
         * @brief Access the gameplay tag registry for tag lookups and validation.
         * @pre Initialise() must have completed; will terminate if subsystem is missing.
         */
        GameplayTagRegistry& GetTagRegistry()
        {
            return GameSubsystems::Get<GameplayTagRegistry>();
        }
        /// @copydoc GetTagRegistry()
        const GameplayTagRegistry& GetTagRegistry() const
        {
            return GameSubsystems::Get<GameplayTagRegistry>();
        }

        Scene* GetCurrentScene()
        {
            return m_currentScene.get();
        }
        const Scene* GetCurrentScene() const
        {
            return m_currentScene.get();
        }
        std::shared_ptr<AssetService> GetAssetService() const
        {
            return m_assetService;
        }

        flecs::world& GetWorld()
        {
            return m_world;
        }
        const flecs::world& GetWorld() const
        {
            return m_world;
        }

        void SetRunning(bool isRunning)
        {
            m_running = isRunning;
        }
        bool IsRunning() const
        {
            return m_running;
        }

    private:
        void InitialiseWorld();
        void InitialiseSubsystems();
        void InitialiseTagRegistry();

        flecs::world m_world;
        SubsystemCollection<GameSubsystem> m_subsystems;
        RuntimeComponentRegistry m_componentRegistry;
        std::unique_ptr<Scene> m_currentScene;
        std::shared_ptr<AssetService> m_assetService;
        const ModuleRegistry* m_moduleRegistry = nullptr;
        GameStateMachine* m_stateMachine = nullptr; ///< Non-owning; owned by m_subsystems.
        bool m_running = false;
        bool m_initialised = false;
    };

} // namespace Wayfinder
