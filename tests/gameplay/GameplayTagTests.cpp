#include "gameplay/GameplayTag.h"

#include <doctest/doctest.h>

using namespace Wayfinder;

// ── Tag Creation ─────────────────────────────────────────
namespace Wayfinder::Tests
{
    TEST_CASE("GameplayTag::None is invalid")
    {
        auto tag = GameplayTag::None();
        CHECK_FALSE(tag.IsValid());
    }

    TEST_CASE("GameplayTag::FromName creates a valid tag")
    {
        auto tag = GameplayTag::FromName("Status.Burning");
        CHECK(tag.IsValid());
        CHECK(tag.GetName() == "Status.Burning");
    }

    TEST_CASE("Tags with the same name are equal")
    {
        auto a = GameplayTag::FromName("Status.Burning");
        auto b = GameplayTag::FromName("Status.Burning");
        CHECK(a == b);
    }

    TEST_CASE("Tags with different names are not equal")
    {
        auto a = GameplayTag::FromName("Status.Burning");
        auto b = GameplayTag::FromName("Status.Frozen");
        CHECK(a != b);
    }

    // ── Hierarchy ────────────────────────────────────────────

    TEST_CASE("IsChildOf returns true for exact match")
    {
        auto tag = GameplayTag::FromName("Status.Burning");
        auto parent = GameplayTag::FromName("Status.Burning");
        CHECK(tag.IsChildOf(parent));
    }

    TEST_CASE("IsChildOf returns true for direct parent")
    {
        auto child = GameplayTag::FromName("Status.Burning");
        auto parent = GameplayTag::FromName("Status");
        CHECK(child.IsChildOf(parent));
    }

    TEST_CASE("IsChildOf returns true for deep hierarchy")
    {
        auto grandchild = GameplayTag::FromName("Status.Damage.Fire");
        auto root = GameplayTag::FromName("Status");
        CHECK(grandchild.IsChildOf(root));
    }

    TEST_CASE("IsChildOf returns false for non-parent")
    {
        auto a = GameplayTag::FromName("Status.Burning");
        auto b = GameplayTag::FromName("Ability");
        CHECK_FALSE(a.IsChildOf(b));
    }

    TEST_CASE("IsChildOf returns false when parent name is prefix but not ancestor")
    {
        auto tag = GameplayTag::FromName("StatusEffect.Burning");
        auto notParent = GameplayTag::FromName("Status");
        CHECK_FALSE(tag.IsChildOf(notParent));
    }

    TEST_CASE("IsChildOf returns false when child is shorter than parent")
    {
        auto parent = GameplayTag::FromName("Status.Burning");
        auto notChild = GameplayTag::FromName("Status");
        CHECK_FALSE(notChild.IsChildOf(parent));
    }

    // ── Parent ───────────────────────────────────────────────

    TEST_CASE("Parent of root tag returns nullopt")
    {
        auto root = GameplayTag::FromName("Status");
        auto parent = root.Parent();
        CHECK_FALSE(parent.has_value());
    }

    TEST_CASE("Parent of child tag returns parent")
    {
        auto child = GameplayTag::FromName("Status.Burning");
        auto parent = child.Parent();
        REQUIRE(parent.has_value());
        CHECK(parent->GetName() == "Status");
    }

    TEST_CASE("Parent of deep tag returns immediate parent")
    {
        auto deep = GameplayTag::FromName("Status.Damage.Fire");
        auto parent = deep.Parent();
        REQUIRE(parent.has_value());
        CHECK(parent->GetName() == "Status.Damage");
    }

    // ── Depth ────────────────────────────────────────────────

    TEST_CASE("Depth of root tag is 1")
    {
        auto tag = GameplayTag::FromName("Status");
        CHECK(tag.Depth() == 1);
    }

    TEST_CASE("Depth of child tag is 2")
    {
        auto tag = GameplayTag::FromName("Status.Burning");
        CHECK(tag.Depth() == 2);
    }

    TEST_CASE("Depth of deep tag is 3")
    {
        auto tag = GameplayTag::FromName("Status.Damage.Fire");
        CHECK(tag.Depth() == 3);
    }

    TEST_CASE("Depth of None tag is 0")
    {
        auto tag = GameplayTag::None();
        CHECK(tag.Depth() == 0);
    }

    // ── Tag Ordering ─────────────────────────────────────────

    TEST_CASE("Tags are orderable via operator<")
    {
        auto a = GameplayTag::FromName("Ability");
        auto b = GameplayTag::FromName("Status");
        CHECK(a < b);
        CHECK_FALSE(b < a);
    }

    // ── GameplayTagContainer ─────────────────────────────────

    TEST_CASE("Empty container")
    {
        GameplayTagContainer container;
        CHECK(container.IsEmpty());
        CHECK(container.Size() == 0);
    }

    TEST_CASE("AddTag and HasExact")
    {
        GameplayTagContainer container;
        auto tag = GameplayTag::FromName("Status.Burning");

        container.AddTag(tag);
        CHECK_FALSE(container.IsEmpty());
        CHECK(container.Size() == 1);
        CHECK(container.HasExact(tag));
    }

    TEST_CASE("AddTag is idempotent")
    {
        GameplayTagContainer container;
        auto tag = GameplayTag::FromName("Status.Burning");

        container.AddTag(tag);
        container.AddTag(tag);
        CHECK(container.Size() == 1);
    }

    TEST_CASE("RemoveTag")
    {
        GameplayTagContainer container;
        auto tag = GameplayTag::FromName("Status.Burning");

        container.AddTag(tag);
        container.RemoveTag(tag);
        CHECK(container.IsEmpty());
        CHECK_FALSE(container.HasExact(tag));
    }

    TEST_CASE("RemoveTag on missing tag is safe")
    {
        GameplayTagContainer container;
        auto tag = GameplayTag::FromName("Status.Burning");
        CHECK_NOTHROW(container.RemoveTag(tag));
    }

    TEST_CASE("HasTag matches descendants")
    {
        GameplayTagContainer container;
        container.AddTag(GameplayTag::FromName("Status.Burning"));

        CHECK(container.HasTag(GameplayTag::FromName("Status")));
        CHECK(container.HasTag(GameplayTag::FromName("Status.Burning")));
        CHECK_FALSE(container.HasTag(GameplayTag::FromName("Ability")));
    }

    TEST_CASE("HasExact does not match parent")
    {
        GameplayTagContainer container;
        container.AddTag(GameplayTag::FromName("Status.Burning"));

        CHECK_FALSE(container.HasExact(GameplayTag::FromName("Status")));
    }

    TEST_CASE("HasAny returns true when at least one matches")
    {
        GameplayTagContainer container;
        container.AddTag(GameplayTag::FromName("Status.Burning"));
        container.AddTag(GameplayTag::FromName("Ability.Fireball"));

        GameplayTagContainer query;
        query.AddTag(GameplayTag::FromName("Status"));
        query.AddTag(GameplayTag::FromName("Movement"));

        CHECK(container.HasAny(query));
    }

    TEST_CASE("HasAny returns false when none match")
    {
        GameplayTagContainer container;
        container.AddTag(GameplayTag::FromName("Status.Burning"));

        GameplayTagContainer query;
        query.AddTag(GameplayTag::FromName("Ability"));

        CHECK_FALSE(container.HasAny(query));
    }

    TEST_CASE("HasAll returns true when all match")
    {
        GameplayTagContainer container;
        container.AddTag(GameplayTag::FromName("Status.Burning"));
        container.AddTag(GameplayTag::FromName("Ability.Fireball"));

        GameplayTagContainer query;
        query.AddTag(GameplayTag::FromName("Status"));
        query.AddTag(GameplayTag::FromName("Ability"));

        CHECK(container.HasAll(query));
    }

    TEST_CASE("HasAll returns false when not all match")
    {
        GameplayTagContainer container;
        container.AddTag(GameplayTag::FromName("Status.Burning"));

        GameplayTagContainer query;
        query.AddTag(GameplayTag::FromName("Status"));
        query.AddTag(GameplayTag::FromName("Ability"));

        CHECK_FALSE(container.HasAll(query));
    }

    TEST_CASE("HasAll with empty query returns true")
    {
        GameplayTagContainer container;
        container.AddTag(GameplayTag::FromName("Status.Burning"));

        GameplayTagContainer emptyQuery;
        CHECK(container.HasAll(emptyQuery));
    }

    TEST_CASE("HasAny with empty query returns false")
    {
        GameplayTagContainer container;
        container.AddTag(GameplayTag::FromName("Status.Burning"));

        GameplayTagContainer emptyQuery;
        CHECK_FALSE(container.HasAny(emptyQuery));
    }

    // ── Container Iteration ──────────────────────────────────

    TEST_CASE("Container is iterable")
    {
        GameplayTagContainer container;
        container.AddTag(GameplayTag::FromName("A"));
        container.AddTag(GameplayTag::FromName("B"));
        container.AddTag(GameplayTag::FromName("C"));

        int count = 0;
        for ([[maybe_unused]] const auto& tag : container)
            ++count;

        CHECK(count == 3);
    }
}