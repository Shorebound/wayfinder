#pragma once

#include "InternedString.h"
#include "wayfinder_exports.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Wayfinder
{
    /**
     * @struct GameplayTag
     * @brief A hierarchical, dot-separated tag used for gameplay classification.
     *
     * Tags follow a "Parent.Child" convention: "Status.Burning" is a child of
     * "Status". Matching queries can test exact equality or parent containment.
     *
     * Tags must be obtained from a GameplayTagRegistry via RequestTag() to
     * ensure they are validated against the project's tag definitions.
     */
    struct WAYFINDER_API GameplayTag
    {
        bool operator==(const GameplayTag& other) const { return m_name == other.m_name; }
        bool operator!=(const GameplayTag& other) const { return m_name != other.m_name; }
        bool operator<(const GameplayTag& other) const { return m_name < other.m_name; }

        const std::string& GetName() const { return m_name.GetString(); }
        bool IsValid() const { return !m_name.IsEmpty(); }

        /// True if this tag equals or is a descendant of @p parent.
        /// "Status.Burning".IsChildOf("Status") -> true
        /// "Status.Burning".IsChildOf("Status.Burning") -> true
        bool IsChildOf(const GameplayTag& parent) const;

        /// Return the immediate parent tag, or nullopt for root-level tags.
        /// "Status.Burning" -> GameplayTag{"Status"}
        std::optional<GameplayTag> Parent() const;

        /// Hierarchy depth (number of dot-separated segments).
        int Depth() const;

        /// Returns an empty/invalid tag.
        static GameplayTag None() { return {}; }

        /// Construct a tag from a pre-interned string.  Used by registries
        /// and internal code that already holds an InternedString.
        static GameplayTag FromInterned(InternedString name) { return GameplayTag{name}; }

        /// Construct a tag from a plain name string.  Interns the string
        /// and returns the tag.
        static GameplayTag FromName(const std::string& name) { return GameplayTag{name}; }

    private:
        explicit GameplayTag(const std::string& name) : m_name(InternedString::Intern(name)) {}
        explicit GameplayTag(InternedString name) : m_name(name) {}
        GameplayTag() = default;

        InternedString m_name;
    };

    /**
     * @struct GameplayTagContainer
     * @brief An ordered set of gameplay tags with hierarchical matching queries.
     */
    struct WAYFINDER_API GameplayTagContainer
    {
        /// True if the container holds this exact tag.
        bool HasExact(const GameplayTag& tag) const;

        /// True if the container holds the tag or any descendant of it.
        bool HasTag(const GameplayTag& tag) const;

        /// True if any tag in @p other is present (exact or descendant).
        bool HasAny(const GameplayTagContainer& other) const;

        /// True if every tag in @p other is present (exact or descendant).
        bool HasAll(const GameplayTagContainer& other) const;

        void AddTag(const GameplayTag& tag);
        void RemoveTag(const GameplayTag& tag);
        bool IsEmpty() const { return m_tags.empty(); }
        size_t Size() const { return m_tags.size(); }

        auto begin() const { return m_tags.begin(); }
        auto end() const { return m_tags.end(); }
        auto cbegin() const { return m_tags.cbegin(); }
        auto cend() const { return m_tags.cend(); }

    private:
        std::vector<GameplayTag> m_tags;
    };

    /**
     * @struct ActiveGameplayTags
     * @brief World singleton holding broad game-state tags.
     *
     * Updated by game code via Game::AddGameplayTag / RemoveGameplayTag.
     * Queried by RunCondition helpers (HasTag, HasAnyTag).
     */
    struct WAYFINDER_API ActiveGameplayTags
    {
        GameplayTagContainer Tags;
    };

} // namespace Wayfinder
