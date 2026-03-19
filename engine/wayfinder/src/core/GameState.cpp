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
        // Build a query container that holds the tag. GameplayTagContainer
        // is a friend of GameplayTag so it can construct via FromName.
        GameplayTagContainer query;
        query.AddTagByName(tagName);
        GameplayTag tag = query.Tags.front();

        return [t = std::move(tag)](const flecs::world& world) -> bool
        {
            const ActiveGameplayTags* tags = world.try_get<ActiveGameplayTags>();
            return tags && tags->Tags.HasTag(t);
        };
    }

    RunCondition HasAnyTag(std::vector<std::string> tagNames)
    {
        GameplayTagContainer query;
        for (auto& name : tagNames)
            query.AddTagByName(name);

        return [q = std::move(query)](const flecs::world& world) -> bool
        {
            const ActiveGameplayTags* tags = world.try_get<ActiveGameplayTags>();
            return tags && tags->Tags.HasAny(q);
        };
    }

} // namespace Wayfinder
