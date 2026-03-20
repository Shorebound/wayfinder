#include "core/Identifiers.h"

#include <doctest/doctest.h>

#include <optional>
#include <string>
#include <unordered_set>

using namespace Wayfinder;

// ── UUID Generation ──────────────────────────────────────

TEST_CASE("Generated UUID is not nil")
{
    auto id = Uuid::Generate();
    CHECK_FALSE(id.IsNil());
    CHECK(static_cast<bool>(id));
}

TEST_CASE("Two generated UUIDs are distinct")
{
    auto a = Uuid::Generate();
    auto b = Uuid::Generate();
    CHECK(a != b);
}

TEST_CASE("Default-constructed UUID is nil")
{
    Uuid id;
    CHECK(id.IsNil());
    CHECK_FALSE(static_cast<bool>(id));
}

// ── UUID Parse / ToString Round-Trip ─────────────────────

TEST_CASE("UUID parse-toString round-trip")
{
    auto original = Uuid::Generate();
    std::string text = original.ToString();

    CHECK(text.size() == 36);

    auto parsed = Uuid::Parse(text);
    REQUIRE(parsed.has_value());
    CHECK(*parsed == original);
}

TEST_CASE("UUID parse rejects invalid input")
{
    CHECK_FALSE(Uuid::Parse("not-a-uuid").has_value());
    CHECK_FALSE(Uuid::Parse("").has_value());
    CHECK_FALSE(Uuid::Parse("00000000-0000-0000-0000").has_value());
    CHECK_FALSE(Uuid::Parse("ZZZZZZZZ-ZZZZ-ZZZZ-ZZZZ-ZZZZZZZZZZZZ").has_value());
}

TEST_CASE("Nil UUID toString produces all-zeros")
{
    Uuid nil;
    std::string text = nil.ToString();
    CHECK(text == "00000000-0000-0000-0000-000000000000");
}

// ── UUID Hashing ─────────────────────────────────────────

TEST_CASE("UUID works in unordered_set")
{
    auto a = Uuid::Generate();
    auto b = Uuid::Generate();

    std::unordered_set<Uuid> set;
    set.insert(a);
    set.insert(b);
    set.insert(a); // duplicate

    CHECK(set.size() == 2);
    CHECK(set.contains(a));
    CHECK(set.contains(b));
}

// ── TypedId ──────────────────────────────────────────────

TEST_CASE("SceneObjectId and AssetId are distinct types")
{
    // Compile-time proof via type traits
    static_assert(!std::is_same_v<SceneObjectId, AssetId>,
                  "SceneObjectId and AssetId must be distinct types");
}

TEST_CASE("TypedId generate and equality")
{
    auto a = SceneObjectId::Generate();
    auto b = SceneObjectId::Generate();
    CHECK(a != b);
    CHECK(a == a);
}

TEST_CASE("TypedId parse round-trip")
{
    auto original = AssetId::Generate();
    std::string text = original.ToString();

    auto parsed = AssetId::Parse(text);
    REQUIRE(parsed.has_value());
    CHECK(*parsed == original);
}

TEST_CASE("Default TypedId is nil")
{
    SceneObjectId id;
    CHECK(id.IsNil());
    CHECK_FALSE(static_cast<bool>(id));
}

TEST_CASE("TypedId works in unordered_set")
{
    auto a = SceneObjectId::Generate();
    auto b = SceneObjectId::Generate();

    std::unordered_set<SceneObjectId> set;
    set.insert(a);
    set.insert(b);
    set.insert(a); // duplicate

    CHECK(set.size() == 2);
}

// ── StringHash ───────────────────────────────────────────

TEST_CASE("StringHash is constexpr")
{
    constexpr StringHash h{"hello"};
    static_assert(h.IsValid(), "Non-empty string should produce a valid hash");

    constexpr StringHash empty{""};
    // FNV-1a of empty string is the basis, which is non-zero
    static_assert(empty.IsValid(), "Empty string FNV-1a is the basis offset, which is non-zero");
}

TEST_CASE("StringHash equality")
{
    constexpr StringHash a{"alpha"};
    constexpr StringHash b{"alpha"};
    constexpr StringHash c{"beta"};

    CHECK(a == b);
    CHECK(a != c);
}

TEST_CASE("Default StringHash is invalid")
{
    StringHash h;
    CHECK_FALSE(h.IsValid());
    CHECK(h.Value == 0);
}

TEST_CASE("StringHash works in unordered_set")
{
    StringHash a{"key1"};
    StringHash b{"key2"};

    std::unordered_set<StringHash> set;
    set.insert(a);
    set.insert(b);
    set.insert(a);

    CHECK(set.size() == 2);
}

TEST_CASE("FNV-1a collision resistance spot-check")
{
    // Common short strings should not collide
    constexpr auto h1 = Fnv1a64("foo");
    constexpr auto h2 = Fnv1a64("bar");
    constexpr auto h3 = Fnv1a64("baz");
    constexpr auto h4 = Fnv1a64("qux");

    CHECK(h1 != h2);
    CHECK(h1 != h3);
    CHECK(h1 != h4);
    CHECK(h2 != h3);
    CHECK(h2 != h4);
    CHECK(h3 != h4);
}
