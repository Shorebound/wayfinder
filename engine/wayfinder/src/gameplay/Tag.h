#pragma once

#include "core/InternedString.h"
#include "wayfinder_exports.h"

#include <compare>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Wayfinder
{
    class TagRegistry;

    namespace Plugins
    {
        class PluginRegistry;
    }

    /**
     * @struct Tag
     * @brief A hierarchical, dot-separated tag used for classification.
     *
     * Tags follow a "Parent.Child" convention: "Status.Burning" is a child of
     * "Status". Matching queries can test exact equality or parent containment.
     *
     * Tags must be obtained from a TagRegistry via RequestTag() to
     * ensure they are validated against the project's tag definitions.
     */
    struct WAYFINDER_API Tag
    {
        bool operator==(const Tag& other) const
        {
            return m_name == other.m_name;
        }
        std::strong_ordering operator<=>(const Tag& other) const
        {
            return GetName() <=> other.GetName();
        }

        std::string_view GetName() const
        {
            return m_name.AsStringView();
        }
        bool IsValid() const
        {
            return !m_name.IsEmpty();
        }

        /// True if this tag equals or is a descendant of @p parent.
        /// "Status.Burning".IsChildOf("Status") -> true
        /// "Status.Burning".IsChildOf("Status.Burning") -> true
        bool IsChildOf(const Tag& parent) const;

        /// Return the immediate parent tag, or nullopt for root-level tags.
        /// "Status.Burning" -> Tag{"Status"}
        std::optional<Tag> Parent() const;

        /// Hierarchy depth (number of dot-separated segments).
        int Depth() const;

        /// Returns an empty/invalid tag.
        static Tag None()
        {
            return {};
        }

    private:
        friend class TagRegistry;
        friend class Plugins::PluginRegistry;
        friend class NativeTag;

        explicit Tag(const std::string_view name) : m_name(InternedString::Intern(name)) {}
        explicit Tag(InternedString name) : m_name(name) {}
        Tag() = default;

        InternedString m_name;
    };

    /**
     * @struct TagContainer
     * @brief An ordered set of tags with hierarchical matching queries.
     */
    struct WAYFINDER_API TagContainer
    {
        /// True if the container holds this exact tag.
        bool HasExact(const Tag& tag) const;

        /// True if the container holds the tag or any descendant of it.
        bool HasTag(const Tag& tag) const;

        /// True if any tag in @p other is present (exact or descendant).
        bool HasAny(const TagContainer& other) const;

        /// True if every tag in @p other is present (exact or descendant).
        bool HasAll(const TagContainer& other) const;

        void AddTag(const Tag& tag);
        void RemoveTag(const Tag& tag);

        /// Merge all tags from @p other into this container.
        void AddTags(const TagContainer& other);

        bool IsEmpty() const
        {
            return m_tags.empty();
        }
        size_t Size() const
        {
            return m_tags.size();
        }

        auto begin() const
        {
            return m_tags.begin();
        }
        auto end() const
        {
            return m_tags.end();
        }
        auto cbegin() const
        {
            return m_tags.cbegin();
        }
        auto cend() const
        {
            return m_tags.cend();
        }

    private:
        std::vector<Tag> m_tags;
    };

    /**
     * @struct ActiveTags
     * @brief World singleton holding broad game-state tags.
     *
     * Updated by game code via Game::AddTag / RemoveTag.
     * Queried by RunCondition helpers (HasTag, HasAnyTag).
     */
    struct WAYFINDER_API ActiveTags
    {
        TagContainer Tags;
    };

} // namespace Wayfinder
