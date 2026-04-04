#include "gameplay/Tag.h"
#include "gameplay/TagRegistry.h"

#include <doctest/doctest.h>

#include <string>

using namespace Wayfinder;

// -- Tag Creation ---------------------------------------------------------
namespace Wayfinder::Tests
{
    TEST_CASE("Tag::None is invalid")
    {
        auto tag = Tag::None();
        CHECK_FALSE(tag.IsValid());
    }

    TEST_CASE("Registry creates a valid tag")
    {
        TagRegistry registry;
        auto tag = registry.RegisterTag("Status.Burning");
        CHECK(tag.IsValid());
        CHECK(std::string(tag.GetName()) == "Status.Burning");
    }

    TEST_CASE("Tags with the same name are equal")
    {
        TagRegistry registry;
        auto a = registry.RegisterTag("Status.Burning");
        auto b = registry.RequestTag("Status.Burning");
        CHECK(a == b);
    }

    TEST_CASE("Tags with different names are not equal")
    {
        TagRegistry registry;
        auto a = registry.RegisterTag("Status.Burning");
        auto b = registry.RegisterTag("Status.Frozen");
        CHECK(a != b);
    }

    // -- Hierarchy --------------------------------------------------------

    TEST_CASE("IsChildOf returns true for exact match")
    {
        TagRegistry registry;
        auto tag = registry.RegisterTag("Status.Burning");
        auto parent = registry.RequestTag("Status.Burning");
        CHECK(tag.IsChildOf(parent));
    }

    TEST_CASE("IsChildOf returns true for direct parent")
    {
        TagRegistry registry;
        auto child = registry.RegisterTag("Status.Burning");
        auto parent = registry.RequestTag("Status");
        CHECK(child.IsChildOf(parent));
    }

    TEST_CASE("IsChildOf returns true for deep hierarchy")
    {
        TagRegistry registry;
        auto grandchild = registry.RegisterTag("Status.Damage.Fire");
        auto root = registry.RequestTag("Status");
        CHECK(grandchild.IsChildOf(root));
    }

    TEST_CASE("IsChildOf returns false for non-parent")
    {
        TagRegistry registry;
        auto a = registry.RegisterTag("Status.Burning");
        auto b = registry.RegisterTag("Ability");
        CHECK_FALSE(a.IsChildOf(b));
    }

    TEST_CASE("IsChildOf returns false when parent name is prefix but not ancestor")
    {
        TagRegistry registry;
        auto tag = registry.RegisterTag("StatusEffect.Burning");
        auto notParent = registry.RegisterTag("Status");
        CHECK_FALSE(tag.IsChildOf(notParent));
    }

    TEST_CASE("IsChildOf returns false when child is shorter than parent")
    {
        TagRegistry registry;
        auto parent = registry.RegisterTag("Status.Burning");
        auto notChild = registry.RequestTag("Status");
        CHECK_FALSE(notChild.IsChildOf(parent));
    }

    // -- Parent -----------------------------------------------------------

    TEST_CASE("Parent of root tag returns nullopt")
    {
        TagRegistry registry;
        auto root = registry.RegisterTag("Status");
        auto parent = root.Parent();
        CHECK_FALSE(parent.has_value());
    }

    TEST_CASE("Parent of child tag returns parent")
    {
        TagRegistry registry;
        auto child = registry.RegisterTag("Status.Burning");
        auto parent = child.Parent();
        REQUIRE(parent.has_value());
        CHECK(std::string(parent->GetName()) == "Status");
    }

    TEST_CASE("Parent of deep tag returns immediate parent")
    {
        TagRegistry registry;
        auto deep = registry.RegisterTag("Status.Damage.Fire");
        auto parent = deep.Parent();
        REQUIRE(parent.has_value());
        CHECK(std::string(parent->GetName()) == "Status.Damage");
    }

    // -- Depth ------------------------------------------------------------

    TEST_CASE("Depth of root tag is 1")
    {
        TagRegistry registry;
        auto tag = registry.RegisterTag("Status");
        CHECK(tag.Depth() == 1);
    }

    TEST_CASE("Depth of child tag is 2")
    {
        TagRegistry registry;
        auto tag = registry.RegisterTag("Status.Burning");
        CHECK(tag.Depth() == 2);
    }

    TEST_CASE("Depth of deep tag is 3")
    {
        TagRegistry registry;
        auto tag = registry.RegisterTag("Status.Damage.Fire");
        CHECK(tag.Depth() == 3);
    }

    TEST_CASE("Depth of None tag is 0")
    {
        auto tag = Tag::None();
        CHECK(tag.Depth() == 0);
    }

    // -- Tag Ordering -----------------------------------------------------

    TEST_CASE("Tags are orderable via operator<")
    {
        TagRegistry registry;
        auto a = registry.RegisterTag("Ability");
        auto b = registry.RegisterTag("Status");
        CHECK(a < b);
        CHECK_FALSE(b < a);
    }

    // -- TagContainer -----------------------------------------------------

    TEST_CASE("Empty container")
    {
        TagContainer container;
        CHECK(container.IsEmpty());
        CHECK(container.Size() == 0);
    }

    TEST_CASE("AddTag and HasExact")
    {
        TagRegistry registry;
        TagContainer container;
        auto tag = registry.RegisterTag("Status.Burning");

        container.AddTag(tag);
        CHECK_FALSE(container.IsEmpty());
        CHECK(container.Size() == 1);
        CHECK(container.HasExact(tag));
    }

    TEST_CASE("AddTag is idempotent")
    {
        TagRegistry registry;
        TagContainer container;
        auto tag = registry.RegisterTag("Status.Burning");

        container.AddTag(tag);
        container.AddTag(tag);
        CHECK(container.Size() == 1);
    }

    TEST_CASE("RemoveTag")
    {
        TagRegistry registry;
        TagContainer container;
        auto tag = registry.RegisterTag("Status.Burning");

        container.AddTag(tag);
        container.RemoveTag(tag);
        CHECK(container.IsEmpty());
        CHECK_FALSE(container.HasExact(tag));
    }

    TEST_CASE("RemoveTag on missing tag is safe")
    {
        TagRegistry registry;
        TagContainer container;
        auto tag = registry.RegisterTag("Status.Burning");
        CHECK_NOTHROW(container.RemoveTag(tag));
    }

    TEST_CASE("HasTag matches descendants")
    {
        TagRegistry registry;
        TagContainer container;
        container.AddTag(registry.RegisterTag("Status.Burning"));

        CHECK(container.HasTag(registry.RequestTag("Status")));
        CHECK(container.HasTag(registry.RequestTag("Status.Burning")));
        CHECK_FALSE(container.HasTag(registry.RegisterTag("Ability")));
    }

    TEST_CASE("HasExact does not match parent")
    {
        TagRegistry registry;
        TagContainer container;
        container.AddTag(registry.RegisterTag("Status.Burning"));

        CHECK_FALSE(container.HasExact(registry.RequestTag("Status")));
    }

    TEST_CASE("HasAny returns true when at least one matches")
    {
        TagRegistry registry;
        TagContainer container;
        container.AddTag(registry.RegisterTag("Status.Burning"));
        container.AddTag(registry.RegisterTag("Ability.Fireball"));

        TagContainer query;
        query.AddTag(registry.RequestTag("Status"));
        query.AddTag(registry.RegisterTag("Movement"));

        CHECK(container.HasAny(query));
    }

    TEST_CASE("HasAny returns false when none match")
    {
        TagRegistry registry;
        TagContainer container;
        container.AddTag(registry.RegisterTag("Status.Burning"));

        TagContainer query;
        query.AddTag(registry.RegisterTag("Ability"));

        CHECK_FALSE(container.HasAny(query));
    }

    TEST_CASE("HasAll returns true when all match")
    {
        TagRegistry registry;
        TagContainer container;
        container.AddTag(registry.RegisterTag("Status.Burning"));
        container.AddTag(registry.RegisterTag("Ability.Fireball"));

        TagContainer query;
        query.AddTag(registry.RequestTag("Status"));
        query.AddTag(registry.RequestTag("Ability"));

        CHECK(container.HasAll(query));
    }

    TEST_CASE("HasAll returns false when not all match")
    {
        TagRegistry registry;
        TagContainer container;
        container.AddTag(registry.RegisterTag("Status.Burning"));

        TagContainer query;
        query.AddTag(registry.RequestTag("Status"));
        query.AddTag(registry.RegisterTag("Ability"));

        CHECK_FALSE(container.HasAll(query));
    }

    TEST_CASE("HasAll with empty query returns true")
    {
        TagRegistry registry;
        TagContainer container;
        container.AddTag(registry.RegisterTag("Status.Burning"));

        TagContainer emptyQuery;
        CHECK(container.HasAll(emptyQuery));
    }

    TEST_CASE("HasAny with empty query returns false")
    {
        TagRegistry registry;
        TagContainer container;
        container.AddTag(registry.RegisterTag("Status.Burning"));

        TagContainer emptyQuery;
        CHECK_FALSE(container.HasAny(emptyQuery));
    }

    // -- AddTags ----------------------------------------------------------

    TEST_CASE("AddTags merges containers")
    {
        TagRegistry registry;
        TagContainer a;
        a.AddTag(registry.RegisterTag("Status.Burning"));

        TagContainer b;
        b.AddTag(registry.RegisterTag("Ability.Fireball"));

        a.AddTags(b);
        CHECK(a.Size() == 2);
        CHECK(a.HasExact(registry.RequestTag("Status.Burning")));
        CHECK(a.HasExact(registry.RequestTag("Ability.Fireball")));
    }

    TEST_CASE("AddTags with duplicate tags does not create duplicates")
    {
        TagRegistry registry;
        auto burning = registry.RegisterTag("Status.Burning");

        TagContainer a;
        a.AddTag(burning);

        TagContainer b;
        b.AddTag(burning);

        a.AddTags(b);
        CHECK(a.Size() == 1);
    }

    // -- Container Iteration ----------------------------------------------

    TEST_CASE("Container is iterable")
    {
        TagRegistry registry;
        TagContainer container;
        container.AddTag(registry.RegisterTag("A"));
        container.AddTag(registry.RegisterTag("B"));
        container.AddTag(registry.RegisterTag("C"));

        int count = 0;
        for ([[maybe_unused]] const auto& tag : container)
        {
            ++count;
        }

        CHECK(count == 3);
    }
}