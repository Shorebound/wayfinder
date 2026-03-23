#include "GameplayTag.h"
#include "core/InternedString.h"

#include <algorithm>

namespace Wayfinder
{
    // ── GameplayTag ─────────────────────────────────────────────

    bool GameplayTag::IsChildOf(const GameplayTag& parent) const
    {
        if (m_name == parent.m_name)
        {
            return true;
        }
        const auto& name = m_name.GetString();
        const auto& parentName = parent.m_name.GetString();
        if (name.size() <= parentName.size())
        {
            return false;
        }
        return name.starts_with(parentName) && name[parentName.size()] == '.';
    }

    std::optional<GameplayTag> GameplayTag::Parent() const
    {
        const auto& name = m_name.GetString();
        const auto pos = name.rfind('.');
        if (pos == std::string::npos)
        {
            return std::nullopt;
        }
        return GameplayTag::FromName(name.substr(0, pos));
    }

    int GameplayTag::Depth() const
    {
        const auto& name = m_name.GetString();
        if (name.empty())
        {
            return 0;
        }
        return static_cast<int>(std::count(name.begin(), name.end(), '.')) + 1;
    }

    // ── GameplayTagContainer ────────────────────────────────────

    bool GameplayTagContainer::HasExact(const GameplayTag& tag) const
    {
        return std::binary_search(m_tags.begin(), m_tags.end(), tag);
    }

    bool GameplayTagContainer::HasTag(const GameplayTag& tag) const
    {
        return std::any_of(m_tags.begin(), m_tags.end(), [&](const GameplayTag& t)
        {
            return t.IsChildOf(tag);
        });
    }

    bool GameplayTagContainer::HasAny(const GameplayTagContainer& other) const
    {
        return std::any_of(other.m_tags.begin(), other.m_tags.end(), [&](const GameplayTag& t)
        {
            return HasTag(t);
        });
    }

    bool GameplayTagContainer::HasAll(const GameplayTagContainer& other) const
    {
        return std::all_of(other.m_tags.begin(), other.m_tags.end(), [&](const GameplayTag& t)
        {
            return HasTag(t);
        });
    }

    void GameplayTagContainer::AddTag(const GameplayTag& tag)
    {
        auto it = std::lower_bound(m_tags.begin(), m_tags.end(), tag);
        if (it == m_tags.end() || *it != tag)
        {
            m_tags.insert(it, tag);
        }
    }

    void GameplayTagContainer::RemoveTag(const GameplayTag& tag)
    {
        auto it = std::lower_bound(m_tags.begin(), m_tags.end(), tag);
        if (it != m_tags.end() && *it == tag)
        {
            m_tags.erase(it);
        }
    }

} // namespace Wayfinder
