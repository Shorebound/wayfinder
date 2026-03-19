#include "GameplayTag.h"
#include "InternedString.h"

#include <algorithm>

namespace Wayfinder
{
    // ── GameplayTag ─────────────────────────────────────────────

    bool GameplayTag::IsChildOf(const GameplayTag& parent) const
    {
        if (m_name == parent.m_name)
            return true;
        const auto& name = m_name.GetString();
        const auto& parentName = parent.m_name.GetString();
        if (name.size() <= parentName.size())
            return false;
        return name.starts_with(parentName) && name[parentName.size()] == '.';
    }

    std::optional<GameplayTag> GameplayTag::Parent() const
    {
        const auto& name = m_name.GetString();
        const auto pos = name.rfind('.');
        if (pos == std::string::npos)
            return std::nullopt;
        return GameplayTag{InternedString::Intern(name.substr(0, pos))};
    }

    int GameplayTag::Depth() const
    {
        const auto& name = m_name.GetString();
        if (name.empty())
            return 0;
        return static_cast<int>(std::count(name.begin(), name.end(), '.')) + 1;
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
