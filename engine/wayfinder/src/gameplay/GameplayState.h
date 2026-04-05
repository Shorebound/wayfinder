#pragma once

#include "app/IApplicationState.h"
#include "core/Result.h"

#include <memory>
#include <string_view>

namespace Wayfinder
{
    class SceneRenderExtractor;
    class Simulation;
    struct SceneCanvas;

    /**
     * @brief Concrete IApplicationState that wraps Simulation in the v2 lifecycle.
     *
     * GameplayState is the core "real workload" deliverable of Phase 5.
     * It proves the IApplicationState lifecycle with actual ECS simulation.
     *
     * Per D-01: wraps Simulation minimally. Does NOT take on TagRegistry,
     * GameStateMachine, or SubsystemCollection responsibilities.
     *
     * Per D-03: does NOT touch ECS directly. Simulation::Initialise handles
     * ECS singleton setup; GameplayState delegates via Simulation's API.
     *
     * Simulation is NOT owned by GameplayState. It is a state subsystem
     * registered at build time via builder.RegisterStateSubsystem<Simulation>()
     * and created by SubsystemManifest when GameplayState enters. GameplayState
     * just accesses it via context.GetStateSubsystem<Simulation>().
     */
    class GameplayState : public IApplicationState
    {
    public:
        GameplayState();
        ~GameplayState() override;

        GameplayState(const GameplayState&) = delete;
        auto operator=(const GameplayState&) -> GameplayState& = delete;
        GameplayState(GameplayState&&) = delete;
        auto operator=(GameplayState&&) -> GameplayState& = delete;

        /// Access Simulation (already created by SubsystemManifest), create SceneRenderExtractor.
        [[nodiscard]] auto OnEnter(EngineContext& context) -> Result<void> override;

        /// Release extractor, null simulation pointer.
        [[nodiscard]] auto OnExit(EngineContext& context) -> Result<void> override;

        /// Delegate to Simulation::Update(deltaTime).
        void OnUpdate(EngineContext& context, float deltaTime) override;

        /// Call SceneRenderExtractor::Extract to fill SceneCanvas.
        void OnRender(EngineContext& context) override;

        [[nodiscard]] auto GetName() const -> std::string_view override;

    private:
        /// Non-owning pointer to the Simulation state subsystem.
        /// Set in OnEnter, cleared in OnExit.
        Simulation* m_simulation = nullptr;

        /// Gameplay-domain extractor created with Simulation's world.
        std::unique_ptr<SceneRenderExtractor> m_extractor;
    };

} // namespace Wayfinder
