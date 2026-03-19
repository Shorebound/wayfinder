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

    RunCondition HasTag(std::string tagName)
    {
        return [tag = GameplayTag::FromString(std::move(tagName))](const flecs::world& world) -> bool
        {
            const ActiveGameplayTags* tags = world.try_get<ActiveGameplayTags>();
            return tags && tags->Tags.HasTag(tag);
        };
    }

    RunCondition HasAnyTag(std::vector<std::string> tagNames)
    {
        GameplayTagContainer query;
        for (auto& name : tagNames)
            query.AddTag(GameplayTag::FromString(std::move(name)));

        return [q = std::move(query)](const flecs::world& world) -> bool
        {
            const ActiveGameplayTags* tags = world.try_get<ActiveGameplayTags>();
            return tags && tags->Tags.HasAny(q);
        };
    }

} // namespace Wayfinder
