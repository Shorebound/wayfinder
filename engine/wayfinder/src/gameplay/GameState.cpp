#include "GameState.h"
#include "GameplayTag.h"
#include "core/InternedString.h"

#include "ecs/Flecs.h"

namespace Wayfinder
{
    RunCondition InState(const std::string& stateName)
    {
        return [name = InternedString::Intern(stateName)](const flecs::world& world) -> bool
        {
            const auto* state = world.try_get<ActiveGameState>();
            return state && state->Current == name;
        };
    }

    RunCondition NotInState(const std::string& stateName)
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
            for (const auto& t : ts)
            {
                if (activeTags->Tags.HasTag(t))
                {
                    return true;
                }
            }
            return false;
        };
    }

    RunCondition AllOf(std::vector<RunCondition> conditions)
    {
        return [cs = std::move(conditions)](const flecs::world& world) -> bool
        {
            for (const auto& c : cs)
            {
                if (!c(world))
                {
                    return false;
                }
            }
            return true;
        };
    }

    RunCondition AnyOf(std::vector<RunCondition> conditions)
    {
        return [cs = std::move(conditions)](const flecs::world& world) -> bool
        {
            for (const auto& c : cs)
            {
                if (c(world))
                {
                    return true;
                }
            }
            return false;
        };
    }

} // namespace Wayfinder
