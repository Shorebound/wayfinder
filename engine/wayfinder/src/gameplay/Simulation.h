#pragma once

#include "app/StateSubsystem.h"
#include "core/Result.h"
#include "ecs/Flecs.h"

#include <memory>
#include <string_view>

namespace Wayfinder
{
    class EngineContext;
    class Scene;

    /**
     * @brief Thin state-scoped subsystem owning the flecs world and current scene.
     *
     * Simulation is the v2 replacement for Game. It owns only the ECS world and
     * the active Scene. Physics, audio, and other gameplay subsystems are separate
     * StateSubsystems that consume Simulation, not children of it.
     *
     * Sub-states are state-internal: the ApplicationState creates its own
     * StateMachine<TStateId> in OnEnter. Simulation provides GetWorld() for
     * ECS singleton updates (e.g. ActiveGameState).
     */
    class WAYFINDER_API Simulation : public StateSubsystem
    {
    public:
        Simulation();
        ~Simulation() override;

        /// Initialise the simulation subsystem.
        [[nodiscard]] auto Initialise(EngineContext& context) -> Result<void> override;

        /// Shutdown and release resources.
        void Shutdown() override;

        /// Tick the flecs world.
        void Update(float deltaTime);

        [[nodiscard]] auto GetWorld() -> flecs::world&;
        [[nodiscard]] auto GetWorld() const -> const flecs::world&;

        [[nodiscard]] auto GetCurrentScene() -> Scene*;
        [[nodiscard]] auto GetCurrentScene() const -> const Scene*;

        /// @prototype Scene loading stub. Full implementation depends on AssetService integration.
        void LoadScene(std::string_view scenePath);

        void UnloadCurrentScene();

    private:
        flecs::world m_world;
        std::unique_ptr<Scene> m_currentScene;
    };

} // namespace Wayfinder
