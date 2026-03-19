#include "GameState.h"
#include "GameplayTag.h"

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

    RunCondition HasTag(GameplayTag tag)
    {
        return [t = std::move(tag)](const flecs::world& world) -> bool
        {
            const ActiveGameplayTags* tags = world.try_get<ActiveGameplayTags>();
            return tags && tags->Tags.HasTag(t);
        };
    }

    RunCondition HasAnyTag(std::vector<GameplayTag> tags)
    {
        return [ts = std::move(tags)](const flecs::world& world) -> bool
        {
            const ActiveGameplayTags* activeTags = world.try_get<ActiveGameplayTags>();
            if (!activeTags)
                return false;
            for (const auto& t : ts)
            {
                if (activeTags->Tags.HasTag(t))
                    return true;
            }
            return false;
        };
    }

} // namespace Wayfinder
