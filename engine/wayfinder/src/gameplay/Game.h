#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ecs/Flecs.h"

#include "GameState.h"
#include "GameStateMachine.h"
#include "Tag.h"
#include "TagRegistry.h"
#include "app/Subsystem.h"
#include "core/Result.h"
#include "scene/RuntimeComponentRegistry.h"
#include "scene/SceneSettings.h"
#include "wayfinder_exports.h"

namespace Wayfinder::Plugins
{
    class PluginRegistry;
}

namespace Wayfinder
{
    class AssetService;
    class Scene;
    struct GameContext;

    class WAYFINDER_API Game
    {
    public:
        explicit Game(const Plugins::PluginRegistry& pluginRegistry);
        ~Game();

        /**
         * @brief Initialise the game: subsystems, ECS world, and boot scene.
         * @param ctx  GameContext providing the project descriptor and plugin registry.
         * @return A successful Result on success, or an Error if the boot
         *         scene cannot be found, resolved, or loaded.
         */
        Result<void> Initialise(const GameContext& ctx);
        void Update(float deltaTime);
        void Shutdown();

        void LoadScene(std::string_view scenePath);
        void UnloadCurrentScene();

        /// Transition to a named game state. Forwards to GameStateMachine.
        void TransitionTo(std::string_view stateName);

        /// Returns the name of the currently active game state.
        std::string_view GetCurrentState() const;

        /// Add a tag to the world-level active tag set.
        void AddTag(const Tag& tag);

        /// Remove a tag from the world-level active tag set.
        void RemoveTag(const Tag& tag);

        /// Check if a tag is active at the world level.
        bool HasTag(const Tag& tag) const;

        /**
         * @brief Access the gameplay tag registry for tag lookups and validation.
         * @pre Initialise() must have completed; will terminate if subsystem is missing.
         */
        TagRegistry& GetTagRegistry()
        {
            return GameSubsystems::Get<TagRegistry>();
        }
        /// @copydoc GetTagRegistry()
        const TagRegistry& GetTagRegistry() const
        {
            return GameSubsystems::Get<TagRegistry>();
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
        const Plugins::PluginRegistry& m_pluginRegistry;
        GameStateMachine* m_stateMachine = nullptr; ///< Non-owning; owned by m_subsystems.
        bool m_running = false;
        bool m_initialised = false;
    };

} // namespace Wayfinder
