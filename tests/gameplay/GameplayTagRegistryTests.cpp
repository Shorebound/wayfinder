#include "TestHelpers.h"
#include "gameplay/GameplayTagRegistry.h"


#include <doctest/doctest.h>

#include <filesystem>

// ── Code Registration ────────────────────────────────────
namespace Wayfinder::Tests
{
    using Helpers::FixturesDir;
    TEST_SUITE("GameplayTagRegistry")
    {
        TEST_CASE("RegisterTag creates a valid tag")
        {
            GameplayTagRegistry registry;
            auto tag = registry.RegisterTag("Status.Burning", "Entity is on fire");

            CHECK(tag.IsValid());
            CHECK(tag.GetName() == "Status.Burning");
            CHECK(registry.IsRegistered("Status.Burning"));
        }

        TEST_CASE("RegisterTag auto-creates ancestor tags")
        {
            GameplayTagRegistry registry;
            registry.RegisterTag("Status.Damage.Fire", "Fire damage");

            CHECK(registry.IsRegistered("Status"));
            CHECK(registry.IsRegistered("Status.Damage"));
            CHECK(registry.IsRegistered("Status.Damage.Fire"));
        }

        TEST_CASE("Duplicate RegisterTag updates comment and source")
        {
            GameplayTagRegistry registry;
            registry.RegisterTag("Status.Burning", "first comment");
            registry.RegisterTag("Status.Burning", "second comment");

            const auto* def = registry.FindDefinition("Status.Burning");
            REQUIRE(def);
            CHECK(def->Comment == "second comment");
            CHECK(def->SourceFile == "(code)");
        }

        TEST_CASE("Duplicate RegisterTag with empty comment keeps original")
        {
            GameplayTagRegistry registry;
            registry.RegisterTag("Status.Burning", "first comment");
            registry.RegisterTag("Status.Burning");

            const auto* def = registry.FindDefinition("Status.Burning");
            REQUIRE(def);
            CHECK(def->Comment == "first comment");
        }

        TEST_CASE("FindDefinition returns nullptr for unregistered tag")
        {
            GameplayTagRegistry registry;
            CHECK(registry.FindDefinition("NonExistent") == nullptr);
        }

        TEST_CASE("FindDefinition returns correct definition")
        {
            GameplayTagRegistry registry;
            registry.RegisterTag("Ability.Fireball", "A fireball");

            const auto* def = registry.FindDefinition("Ability.Fireball");
            REQUIRE(def);
            CHECK(def->Name == "Ability.Fireball");
            CHECK(def->Comment == "A fireball");
            CHECK(def->SourceFile == "(code)");
        }

        TEST_CASE("IsRegistered returns false for unknown tag")
        {
            GameplayTagRegistry registry;
            CHECK_FALSE(registry.IsRegistered("Unknown.Tag"));
        }

        // ── RequestTag ──────────────────────────────────────────

        TEST_CASE("RequestTag returns a tag even if unregistered")
        {
            GameplayTagRegistry registry;
            auto tag = registry.RequestTag("Unregistered.Tag");
            CHECK(tag.IsValid());
            CHECK(tag.GetName() == "Unregistered.Tag");
        }

        TEST_CASE("RequestTag returns tag for registered name")
        {
            GameplayTagRegistry registry;
            registry.RegisterTag("Status");
            auto tag = registry.RequestTag("Status");
            CHECK(tag.IsValid());
            CHECK(tag.GetName() == "Status");
        }

        // ── TOML File Loading ───────────────────────────────────

        TEST_CASE("LoadTagFile loads tags from TOML")
        {
            GameplayTagRegistry registry;
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
            GameplayTagRegistry registry;
            auto path = FixturesDir() / "test_tags.toml";
            registry.LoadTagFile(path);

            // "Status" is explicitly in the file, and is also an ancestor of Status.Burning
            CHECK(registry.IsRegistered("Status"));
        }

        TEST_CASE("LoadTagFile with empty tags array returns 0")
        {
            GameplayTagRegistry registry;
            auto path = FixturesDir() / "empty_tags.toml";
            int count = registry.LoadTagFile(path);

            CHECK(count == 0);
        }

        TEST_CASE("LoadTagFile with invalid TOML returns -1")
        {
            GameplayTagRegistry registry;
            auto path = FixturesDir() / "bad_tags.toml";
            int count = registry.LoadTagFile(path);

            CHECK(count == -1);
        }

        TEST_CASE("LoadTagFile with non-existent file returns -1")
        {
            GameplayTagRegistry registry;
            int count = registry.LoadTagFile("non_existent_path.toml");
            CHECK(count == -1);
        }

        TEST_CASE("LoadTagFile tracks loaded file paths")
        {
            GameplayTagRegistry registry;
            auto path = FixturesDir() / "test_tags.toml";
            registry.LoadTagFile(path);

            const auto& loaded = registry.GetLoadedFiles();
            CHECK(loaded.size() == 1);
        }

        // ── UnloadTagFile ───────────────────────────────────────

        TEST_CASE("UnloadTagFile removes file-sourced definitions")
        {
            GameplayTagRegistry registry;
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
            GameplayTagRegistry registry;

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
            GameplayTagRegistry registry;
            registry.RegisterTag("A");
            registry.RegisterTag("B");
            registry.RegisterTag("C");

            CHECK(registry.GetAllDefinitions().size() == 3);
        }
    }
}