#include "GameState.h"

#include <flecs.h>

namespace Wayfinder
{
    RunCondition InState(std::string stateName)
    {
        return [name = std::move(stateName)](const flecs::world& world) -> bool
        {
            const ActiveGameState* state = world.try_get<ActiveGameState>();
            return state && state->Current == name;
        };
    }

    RunCondition NotInState(std::string stateName)
    {
        return [name = std::move(stateName)](const flecs::world& world) -> bool
        {
            const ActiveGameState* state = world.try_get<ActiveGameState>();
            return !state || state->Current != name;
        };
    }

} // namespace Wayfinder
