#pragma once

#include "GameplayTag.h"
#include "GameState.h"
#include "Subsystem.h"
#include "wayfinder_exports.h"

#include <string>
#include <string_view>
#include <vector>

namespace flecs
{
    struct world;
}

namespace Wayfinder
{
    class ModuleRegistry;

    /// Manages game-state transitions, run-condition evaluation, and
    /// conditioned-system tracking.  Extracted from Game so that state
    /// logic lives in a focused, testable subsystem.
    ///
    /// Lifecycle:
    ///   1. Game creates GameStateMachine as a GameSubsystem.
    ///   2. Game calls Configure() after subsystem Initialise().
    ///   3. GameStateMachine::Setup() binds conditioned systems and
    ///      performs the initial state transition.
    ///   4. Game::Update() calls Update() each frame before world.progress().
    class WAYFINDER_API GameStateMachine : public GameSubsystem
    {
    public:
        /// Post-initialisation configuration.  Must be called before Setup().
        void Configure(flecs::world& world, const ModuleRegistry* moduleRegistry);

        /// Bind conditioned systems and perform the initial state transition.
        /// Call once after Configure() and after ApplyToWorld().
        void Setup();

        /// Evaluate dirty run conditions.  Called by Game::Update() each
        /// frame before world.progress().
        void Update();

        /// Transition to a named game state.  Calls OnExit for the old
        /// state, updates the ActiveGameState singleton, calls OnEnter for
        /// the new state, and marks run conditions dirty.
        void TransitionTo(const std::string& stateName);

        /// Returns the name of the currently active game state.
        std::string_view GetCurrentState() const;

        /// Mark run conditions as dirty so they are re-evaluated next Update().
        void MarkDirty() { m_runConditionsDirty = true; }

    private:
        struct ConditionedSystem
        {
            flecs::entity SystemEntity;
            RunCondition Condition;
            bool Enabled = true;
        };

        void BindConditionedSystems();
        void EvaluateRunConditions();

        flecs::world* m_world = nullptr;
        const ModuleRegistry* m_moduleRegistry = nullptr;
        std::vector<ConditionedSystem> m_conditionedSystems;
        bool m_runConditionsDirty = false;
    };

} // namespace Wayfinder
