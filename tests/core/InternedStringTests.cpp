#include "core/InternedString.h"

#include <doctest/doctest.h>

#include <format>
#include <string>
#include <unordered_map>
#include <unordered_set>

// ── Interning Basics ─────────────────────────────────────
namespace Wayfinder::Tests
{
    TEST_CASE("Same string interns to same pointer")
    {
        auto a = InternedString::Intern("Hello");
        auto b = InternedString::Intern("Hello");

        CHECK(a == b);
        CHECK(&a.GetString() == &b.GetString());
    }

    TEST_CASE("Different strings intern to different pointers")
    {
        auto a = InternedString::Intern("Alpha");
        auto b = InternedString::Intern("Beta");

        CHECK(a != b);
        CHECK(&a.GetString() != &b.GetString());
    }

    TEST_CASE("Interning preserves string content")
    {
        auto s = InternedString::Intern("GameplayTag.Status.Burning");
        CHECK(s.GetString() == "GameplayTag.Status.Burning");
    }

    TEST_CASE("Interning from std::string works")
    {
        std::string source = "DynamicString";
        auto a = InternedString::Intern(source);
        auto b = InternedString::Intern("DynamicString");

        CHECK(a == b);
    }

    // ── Default / Empty ──────────────────────────────────────

    TEST_CASE("Default-constructed InternedString is empty")
    {
        InternedString s;
        CHECK(s.IsEmpty());
        CHECK(s.GetString().empty());
    }

    TEST_CASE("Empty string interned equals default")
    {
        auto a = InternedString::Intern("");
        InternedString b;

        CHECK(a == b);
    }

    // ── String view / storage ─────────────────────────────────

    TEST_CASE("AsStringView matches GetString content")
    {
        auto s = InternedString::Intern("Convertible");
        CHECK(s.GetString() == std::string(s.AsStringView()));
        CHECK(std::string(s.AsStringView()) == "Convertible");
    }

    TEST_CASE("InternedString compares to string_view without interning rhs")
    {
        auto s = InternedString::Intern("GameplayTag.Status.Burning");
        const std::string_view sv{"GameplayTag.Status.Burning"};
        CHECK(std::string(s.AsStringView()) == std::string(sv));
        CHECK(((s <=> sv) == std::strong_ordering::equal));
        CHECK_FALSE(s.operator==(std::string_view("Other")));
    }

    // ── Ordering ─────────────────────────────────────────────

    TEST_CASE("Content-based ordering via operator<")
    {
        auto alpha = InternedString::Intern("Alpha");
        auto beta = InternedString::Intern("Beta");

        CHECK(alpha < beta);
        CHECK_FALSE(beta < alpha);
    }

    // ── Hashing ──────────────────────────────────────────────

    TEST_CASE("InternedString works in unordered_set")
    {
        auto a = InternedString::Intern("KeyA");
        auto b = InternedString::Intern("KeyB");
        auto aDuplicate = InternedString::Intern("KeyA");

        std::unordered_set<InternedString> set;
        set.insert(a);
        set.insert(b);
        set.insert(aDuplicate); // duplicate of a

        CHECK(set.size() == 2);
        CHECK(set.contains(a));
        CHECK(set.contains(b));
    }

    TEST_CASE("InternedString works as unordered_map key")
    {
        auto key = InternedString::Intern("MapKey");

        std::unordered_map<InternedString, int> map;
        map[key] = 42;

        auto lookup = InternedString::Intern("MapKey");
        CHECK(map.count(lookup) == 1);
        CHECK(map[lookup] == 42);
    }

    // ── std::format ──────────────────────────────────────────

    TEST_CASE("InternedString works with std::format")
    {
        auto s = InternedString::Intern("World");
        std::string result = std::format("Hello, {}!", s);
        CHECK(result == "Hello, World!");
    }

    TEST_CASE("InternedString works with std::format in complex expressions")
    {
        auto layer = InternedString::Intern("main");
        auto pass = InternedString::Intern("main_scene");
        std::string result = std::format("Layer={}, Pass={}", layer, pass);
        CHECK(result == "Layer=main, Pass=main_scene");
    }
}