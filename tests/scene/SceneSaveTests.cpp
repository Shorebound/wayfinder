#include "scene/Components.h"
#include "scene/RuntimeComponentRegistry.h"
#include "scene/Scene.h"
#include "scene/SceneDocument.h"
#include "scene/entity/Entity.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <string>

#include <flecs.h>

using namespace Wayfinder;

namespace
{
    std::filesystem::path FixturesDir()
    {
        return std::filesystem::path(__FILE__).parent_path().parent_path() / "fixtures";
    }

    /// Temporary file path under /tmp for save tests.
    std::filesystem::path TempScenePath()
    {
        return std::filesystem::temp_directory_path() / "wayfinder_test_scene_save.toml";
    }

    RuntimeComponentRegistry MakeTestRegistry()
    {
        RuntimeComponentRegistry registry;
        registry.AddCoreEntries();
        return registry;
    }
}

TEST_SUITE("Scene Save")
{
    TEST_CASE("SaveToFile writes a valid TOML file")
    {
        flecs::world world;
        auto registry = MakeTestRegistry();
        registry.RegisterComponents(world);
        Scene::RegisterCoreECS(world);
        Scene scene(world, registry, "SaveTest");

        auto entity = scene.CreateEntity("TestEntity");
        entity.AddComponent<TransformComponent>(TransformComponent{{1.0f, 2.0f, 3.0f}});

        auto path = TempScenePath();
        bool result = scene.SaveToFile(path.string());

        CHECK(result);
        CHECK(std::filesystem::exists(path));

        // Clean up
        std::filesystem::remove(path);
    }

    TEST_CASE("Save and reload preserves entity count")
    {
        flecs::world world;
        auto registry = MakeTestRegistry();
        registry.RegisterComponents(world);
        Scene::RegisterCoreECS(world);
        Scene scene(world, registry, "RoundTrip");

        scene.CreateEntity("Alpha");
        scene.CreateEntity("Beta");
        scene.CreateEntity("Gamma");

        auto path = TempScenePath();
        REQUIRE(scene.SaveToFile(path.string()));

        // Reload into a new scene
        Scene reloaded(world, registry, "Reloaded");
        REQUIRE(reloaded.LoadFromFile(path.string()));

        auto alpha = reloaded.GetEntityByName("Alpha");
        auto beta = reloaded.GetEntityByName("Beta");
        auto gamma = reloaded.GetEntityByName("Gamma");

        CHECK(alpha.IsValid());
        CHECK(beta.IsValid());
        CHECK(gamma.IsValid());

        std::filesystem::remove(path);
    }

    TEST_CASE("Save and reload preserves scene name")
    {
        flecs::world world;
        auto registry = MakeTestRegistry();
        registry.RegisterComponents(world);
        Scene::RegisterCoreECS(world);

        auto path = FixturesDir() / "test_scene.toml";
        Scene scene(world, registry, "Default");
        REQUIRE(scene.LoadFromFile(path.string()));

        auto savePath = TempScenePath();
        REQUIRE(scene.SaveToFile(savePath.string()));

        Scene reloaded(world, registry, "Reloaded");
        REQUIRE(reloaded.LoadFromFile(savePath.string()));

        CHECK(reloaded.GetName() == "Test Scene");

        std::filesystem::remove(savePath);
    }

    TEST_CASE("Save and reload preserves SceneObjectIds")
    {
        flecs::world world;
        auto registry = MakeTestRegistry();
        registry.RegisterComponents(world);
        Scene::RegisterCoreECS(world);
        Scene scene(world, registry, "IdTest");

        auto entity = scene.CreateEntity("Tracked");
        auto originalId = entity.GetSceneObjectId();

        auto path = TempScenePath();
        REQUIRE(scene.SaveToFile(path.string()));

        Scene reloaded(world, registry, "Reloaded");
        REQUIRE(reloaded.LoadFromFile(path.string()));

        auto found = reloaded.GetEntityByName("Tracked");
        REQUIRE(found.IsValid());
        CHECK(found.GetSceneObjectId() == originalId);

        std::filesystem::remove(path);
    }

    // ── SaveSceneDocument Low-Level ─────────────────────────

    TEST_CASE("SaveSceneDocument writes to file")
    {
        SceneDocument doc;
        doc.Name = "DocTest";

        SceneDocumentEntity entity;
        entity.Id = SceneObjectId::Generate();
        entity.Name = "TestEntity";
        doc.Entities.push_back(std::move(entity));

        auto path = TempScenePath();
        std::string error;
        bool result = SaveSceneDocument(doc, path.string(), error);

        CHECK(result);
        CHECK(error.empty());
        CHECK(std::filesystem::exists(path));

        std::filesystem::remove(path);
    }
}
