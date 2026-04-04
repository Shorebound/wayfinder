#include "Tag.h"
#include "core/InternedString.h"

#include <algorithm>
#include <ranges>

namespace Wayfinder
{
    // -- Tag ------------------------------------------------------------------

    bool Tag::IsChildOf(const Tag& parent) const
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
        return name.starts_with(parentName) && name.compare(parentName.size(), 1, ".") == 0;
    }

    std::optional<Tag> Tag::Parent() const
    {
        const auto& name = m_name.GetString();
        const auto pos = name.rfind('.');
        if (pos == std::string::npos)
        {
            return std::nullopt;
        }
        return Tag{InternedString::Intern(name.substr(0, pos))};
    }

    int Tag::Depth() const
    {
        const auto& name = m_name.GetString();
        if (name.empty())
        {
            return 0;
        }
        return static_cast<int>(std::count(name.begin(), name.end(), '.')) + 1;
    }

    // -- TagContainer ---------------------------------------------------------

    bool TagContainer::HasExact(const Tag& tag) const
    {
        return std::ranges::binary_search(m_tags, tag);
    }

    bool TagContainer::HasTag(const Tag& tag) const
    {
        return std::ranges::any_of(m_tags, [&](const Tag& t)
        {
            return t.IsChildOf(tag);
        });
    }

    bool TagContainer::HasAny(const TagContainer& other) const
    {
        return std::ranges::any_of(other.m_tags, [&](const Tag& t)
        {
            return HasTag(t);
        });
    }

    bool TagContainer::HasAll(const TagContainer& other) const
    {
        return std::ranges::all_of(other.m_tags, [&](const Tag& t)
        {
            return HasTag(t);
        });
    }

    void TagContainer::AddTag(const Tag& tag)
    {
        auto it = std::ranges::lower_bound(m_tags, tag);
        if (it == m_tags.end() || *it != tag)
        {
            m_tags.insert(it, tag);
        }
    }

    void TagContainer::RemoveTag(const Tag& tag)
    {
        auto it = std::ranges::lower_bound(m_tags, tag);
        if (it != m_tags.end() && *it == tag)
        {
            m_tags.erase(it);
        }
    }

    void TagContainer::AddTags(const TagContainer& other)
    {
        for (const auto& tag : other)
        {
            AddTag(tag);
        }
    }

} // namespace Wayfinder
