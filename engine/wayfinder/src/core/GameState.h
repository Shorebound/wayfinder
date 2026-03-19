#pragma once

#include "GameplayTag.h"
#include "wayfinder_exports.h"

#include <functional>
#include <string>
#include <vector>

namespace flecs
{
    struct world;
}

namespace Wayfinder
{
    /// World singleton that tracks the currently active game state.
    /// Set by Game::TransitionTo() and read by RunCondition helpers.
    struct ActiveGameState
    {
        std::string Current;
        std::string Previous;
    };

    /// Predicate evaluated each frame to decide whether a system should be active.
    /// Receives the flecs world (which contains the ActiveGameState singleton)
    /// and returns true if the associated system should run.
    using RunCondition = std::function<bool(const flecs::world&)>;

    /// Returns a run condition that is true when the given state is active.
    WAYFINDER_API RunCondition InState(std::string stateName);

    /// Returns a run condition that is true when the given state is NOT active.
    WAYFINDER_API RunCondition NotInState(std::string stateName);

    /// Returns a run condition that is true when the given gameplay tag is active.
    WAYFINDER_API RunCondition HasTag(GameplayTag tag);

    /// Returns a run condition that is true when any of the given tags is active.
    WAYFINDER_API RunCondition HasAnyTag(std::vector<GameplayTag> tags);

} // namespace Wayfinder
