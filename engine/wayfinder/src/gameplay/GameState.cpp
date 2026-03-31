#include "GameState.h"
#include "GameplayTag.h"
#include "core/InternedString.h"

#include "ecs/Flecs.h"
#include <algorithm>

namespace Wayfinder
{
    RunCondition InState(const std::string_view stateName)
    {
        return [name = InternedString::Intern(stateName)](const flecs::world& world) -> bool
        {
            const auto* state = world.try_get<ActiveGameState>();
            return state && state->Current == name;
        };
    }

    RunCondition NotInState(const std::string_view stateName)
    {
        return [name = InternedString::Intern(stateName)](const flecs::world& world) -> bool
        {
            const auto* state = world.try_get<ActiveGameState>();
            return !state || state->Current != name;
        };
    }

    RunCondition HasTag(GameplayTag tag)
    {
        return [t = tag](const flecs::world& world) -> bool
        {
            const auto* tags = world.try_get<ActiveGameplayTags>();
            return tags && tags->Tags.HasTag(t);
        };
    }

    RunCondition HasAnyTag(std::vector<GameplayTag> tags)
    {
        return [ts = std::move(tags)](const flecs::world& world) -> bool
        {
            const auto* activeTags = world.try_get<ActiveGameplayTags>();
            if (!activeTags)
            {
                return false;
            }

            return std::ranges::any_of(ts, [&](const GameplayTag& t)
            {
                return activeTags->Tags.HasTag(t);
            });
        };
    }

    RunCondition AllOf(std::vector<RunCondition> conditions)
    {
        return [cs = std::move(conditions)](const flecs::world& world) -> bool
        {
            return std::ranges::all_of(cs, [&](const RunCondition& c)
            {
                return c(world);
            });
        };
    }

    RunCondition AnyOf(std::vector<RunCondition> conditions)
    {
        return [cs = std::move(conditions)](const flecs::world& world) -> bool
        {
            return std::ranges::any_of(cs, [&](const RunCondition& c)
            {
                return c(world);
            });
        };
    }

} // namespace Wayfinder
