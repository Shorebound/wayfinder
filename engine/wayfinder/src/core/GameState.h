#pragma once

#include "GameplayTag.h"
#include "InternedString.h"
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
    struct WAYFINDER_API ActiveGameState
    {
        InternedString Current;
        InternedString Previous;
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

    /// Returns a run condition that is true when ALL inner conditions are true.
    WAYFINDER_API RunCondition AllOf(std::vector<RunCondition> conditions);

    /// Returns a run condition that is true when ANY inner condition is true.
    WAYFINDER_API RunCondition AnyOf(std::vector<RunCondition> conditions);

} // namespace Wayfinder
