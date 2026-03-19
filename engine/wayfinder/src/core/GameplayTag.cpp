#include "GameplayTag.h"

#include <algorithm>

namespace Wayfinder
{
    // ── GameplayTag ─────────────────────────────────────────────

    bool GameplayTag::IsChildOf(const GameplayTag& parent) const
    {
        if (Name == parent.Name)
            return true;
        if (Name.size() <= parent.Name.size())
            return false;
        return Name.starts_with(parent.Name) && Name[parent.Name.size()] == '.';
    }

    std::optional<GameplayTag> GameplayTag::Parent() const
    {
        const auto pos = Name.rfind('.');
        if (pos == std::string::npos)
            return std::nullopt;
        return GameplayTag{Name.substr(0, pos)};
    }

    int GameplayTag::Depth() const
    {
        if (Name.empty())
            return 0;
        return static_cast<int>(std::count(Name.begin(), Name.end(), '.')) + 1;
    }

    // ── GameplayTagContainer ────────────────────────────────────

    bool GameplayTagContainer::HasExact(const GameplayTag& tag) const
    {
        return std::binary_search(Tags.begin(), Tags.end(), tag);
    }

    bool GameplayTagContainer::HasTag(const GameplayTag& tag) const
    {
        return std::any_of(Tags.begin(), Tags.end(), [&](const GameplayTag& t)
        {
            return t.IsChildOf(tag);
        });
    }

    bool GameplayTagContainer::HasAny(const GameplayTagContainer& other) const
    {
        return std::any_of(other.Tags.begin(), other.Tags.end(), [&](const GameplayTag& t)
        {
            return HasTag(t);
        });
    }

    bool GameplayTagContainer::HasAll(const GameplayTagContainer& other) const
    {
        return std::all_of(other.Tags.begin(), other.Tags.end(), [&](const GameplayTag& t)
        {
            return HasTag(t);
        });
    }

    void GameplayTagContainer::AddTag(const GameplayTag& tag)
    {
        auto it = std::lower_bound(Tags.begin(), Tags.end(), tag);
        if (it == Tags.end() || *it != tag)
            Tags.insert(it, tag);
    }

    void GameplayTagContainer::RemoveTag(const GameplayTag& tag)
    {
        auto it = std::lower_bound(Tags.begin(), Tags.end(), tag);
        if (it != Tags.end() && *it == tag)
            Tags.erase(it);
    }

} // namespace Wayfinder
