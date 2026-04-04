#include "TestHelpers.h"
#include "gameplay/TagRegistry.h"

#include <doctest/doctest.h>

#include <string>

#include <filesystem>

// ── Code Registration ────────────────────────────────────
namespace Wayfinder::Tests
{
    using Helpers::FixturesDir;
    TEST_SUITE("TagRegistry")
    {
        TEST_CASE("RegisterTag creates a valid tag")
        {
            TagRegistry registry;
            auto tag = registry.RegisterTag("Status.Burning", "Entity is on fire");

            CHECK(tag.IsValid());
            CHECK(std::string(tag.GetName()) == "Status.Burning");
            CHECK(registry.IsRegistered("Status.Burning"));
        }

        TEST_CASE("RegisterTag auto-creates ancestor tags")
        {
            TagRegistry registry;
            registry.RegisterTag("Status.Damage.Fire", "Fire damage");

            CHECK(registry.IsRegistered("Status"));
            CHECK(registry.IsRegistered("Status.Damage"));
            CHECK(registry.IsRegistered("Status.Damage.Fire"));
        }

        TEST_CASE("Duplicate RegisterTag updates comment and source")
        {
            TagRegistry registry;
            registry.RegisterTag("Status.Burning", "first comment");
            registry.RegisterTag("Status.Burning", "second comment");

            const auto* def = registry.FindDefinition("Status.Burning");
            REQUIRE(def);
            CHECK(def->Comment == "second comment");
            CHECK(def->SourceFile == "(code)");
        }

        TEST_CASE("Duplicate RegisterTag with empty comment keeps original")
        {
            TagRegistry registry;
            registry.RegisterTag("Status.Burning", "first comment");
            registry.RegisterTag("Status.Burning");

            const auto* def = registry.FindDefinition("Status.Burning");
            REQUIRE(def);
            CHECK(def->Comment == "first comment");
        }

        TEST_CASE("FindDefinition returns nullptr for unregistered tag")
        {
            TagRegistry registry;
            CHECK(registry.FindDefinition("NonExistent") == nullptr);
        }

        TEST_CASE("FindDefinition returns correct definition")
        {
            TagRegistry registry;
            registry.RegisterTag("Ability.Fireball", "A fireball");

            const auto* def = registry.FindDefinition("Ability.Fireball");
            REQUIRE(def);
            CHECK(def->Name == "Ability.Fireball");
            CHECK(def->Comment == "A fireball");
            CHECK(def->SourceFile == "(code)");
        }

        TEST_CASE("IsRegistered returns false for unknown tag")
        {
            TagRegistry registry;
            CHECK_FALSE(registry.IsRegistered("Unknown.Tag"));
        }

        // ── RequestTag ──────────────────────────────────────────

        TEST_CASE("RequestTag returns None for unregistered name")
        {
            TagRegistry registry;
            auto tag = registry.RequestTag("Unregistered.Tag");
            CHECK_FALSE(tag.IsValid());
        }

        TEST_CASE("RequestTag returns tag for registered name")
        {
            TagRegistry registry;
            registry.RegisterTag("Status");
            auto tag = registry.RequestTag("Status");
            CHECK(tag.IsValid());
            CHECK(std::string(tag.GetName()) == "Status");
        }

        // ── TOML File Loading ───────────────────────────────────

        TEST_CASE("LoadTagFile loads tags from TOML")
        {
            TagRegistry registry;
            auto path = FixturesDir() / "test_tags.toml";
            int count = registry.LoadTagFile(path);

            CHECK(count == 5);
            CHECK(registry.IsRegistered("Status"));
            CHECK(registry.IsRegistered("Status.Burning"));
            CHECK(registry.IsRegistered("Status.Frozen"));
            CHECK(registry.IsRegistered("Ability"));
            CHECK(registry.IsRegistered("Ability.Fireball"));
        }

        TEST_CASE("LoadTagFile auto-creates ancestors from file")
        {
            TagRegistry registry;
            auto path = FixturesDir() / "test_tags.toml";
            registry.LoadTagFile(path);

            // "Status" is explicitly in the file, and is also an ancestor of Status.Burning
            CHECK(registry.IsRegistered("Status"));
        }

        TEST_CASE("LoadTagFile with empty tags array returns 0")
        {
            TagRegistry registry;
            auto path = FixturesDir() / "empty_tags.toml";
            int count = registry.LoadTagFile(path);

            CHECK(count == 0);
        }

        TEST_CASE("LoadTagFile with invalid TOML returns -1")
        {
            TagRegistry registry;
            auto path = FixturesDir() / "bad_tags.toml";
            int count = registry.LoadTagFile(path);

            CHECK(count == -1);
        }

        TEST_CASE("LoadTagFile with non-existent file returns -1")
        {
            TagRegistry registry;
            int count = registry.LoadTagFile("non_existent_path.toml");
            CHECK(count == -1);
        }

        TEST_CASE("LoadTagFile tracks loaded file paths")
        {
            TagRegistry registry;
            auto path = FixturesDir() / "test_tags.toml";
            registry.LoadTagFile(path);

            const auto& loaded = registry.GetLoadedFiles();
            CHECK(loaded.size() == 1);
        }

        // ── UnloadTagFile ───────────────────────────────────────

        TEST_CASE("UnloadTagFile removes file-sourced definitions")
        {
            TagRegistry registry;
            auto path = FixturesDir() / "test_tags.toml";
            registry.LoadTagFile(path);

            REQUIRE(registry.IsRegistered("Status.Burning"));

            registry.UnloadTagFile(path);

            CHECK_FALSE(registry.IsRegistered("Status.Burning"));
            CHECK_FALSE(registry.IsRegistered("Status.Frozen"));
            CHECK(registry.GetLoadedFiles().empty());
        }

        TEST_CASE("UnloadTagFile preserves code-registered tags")
        {
            TagRegistry registry;

            // Register "Status" from code first
            registry.RegisterTag("Status", "Code-owned root");

            auto path = FixturesDir() / "test_tags.toml";
            registry.LoadTagFile(path);

            // Unload file — "Status" should remain because it was code-registered
            registry.UnloadTagFile(path);

            CHECK(registry.IsRegistered("Status"));
            const auto* def = registry.FindDefinition("Status");
            REQUIRE(def);
            CHECK(def->SourceFile == "(code)");
        }

        // ── GetAllDefinitions ───────────────────────────────────

        TEST_CASE("GetAllDefinitions returns all registered tags")
        {
            TagRegistry registry;
            registry.RegisterTag("A");
            registry.RegisterTag("B");
            registry.RegisterTag("C");

            CHECK(registry.GetAllDefinitions().size() == 3);
        }
    }
}