#include "scene/Components.h"
#include "scene/RuntimeComponentRegistry.h"
#include "scene/Scene.h"
#include "scene/SceneDocument.h"
#include "scene/entity/Entity.h"
#include "TestHelpers.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <string>

#include <flecs.h>

using namespace Wayfinder;
using TestHelpers::FixturesDir;
using TestHelpers::MakeTestRegistry;

namespace Wayfinder
{
    /// Temporary file path under the fixtures directory for save tests.
    std::filesystem::path TempScenePath()
    {
        static const auto dir = []()
        {
            auto d = FixturesDir() / "temp";
            std::filesystem::create_directories(d);
            return d;
        }();
        return dir / "wayfinder_test_scene_save.json";
    }
}

TEST_SUITE("Scene Save")
{
    TEST_CASE("SaveToFile writes a valid JSON file")
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
        auto registry = MakeTestRegistry();

        // Save phase
        auto path = TempScenePath();
        {
            flecs::world world;
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);
            Scene scene(world, registry, "RoundTrip");

            scene.CreateEntity("Alpha");
            scene.CreateEntity("Beta");
            scene.CreateEntity("Gamma");

            REQUIRE(scene.SaveToFile(path.string()));
        }

        // Reload into a fresh world so no leftover entities
        {
            flecs::world reloadWorld;
            registry.RegisterComponents(reloadWorld);
            Scene::RegisterCoreECS(reloadWorld);
            Scene reloaded(reloadWorld, registry, "Reloaded");
            REQUIRE(reloaded.LoadFromFile(path.string()));

            auto alpha = reloaded.GetEntityByName("Alpha");
            auto beta = reloaded.GetEntityByName("Beta");
            auto gamma = reloaded.GetEntityByName("Gamma");

            CHECK(alpha.IsValid());
            CHECK(beta.IsValid());
            CHECK(gamma.IsValid());
        }

        std::filesystem::remove(path);
    }

    TEST_CASE("Save and reload preserves scene name")
    {
        auto registry = MakeTestRegistry();
        auto savePath = TempScenePath();

        // Load and save phase
        {
            flecs::world world;
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);

            auto path = FixturesDir() / "test_scene.json";
            Scene scene(world, registry, "Default");
            REQUIRE(scene.LoadFromFile(path.string()));
            REQUIRE(scene.SaveToFile(savePath.string()));
        }

        // Reload into a fresh world
        {
            flecs::world reloadWorld;
            registry.RegisterComponents(reloadWorld);
            Scene::RegisterCoreECS(reloadWorld);

            Scene reloaded(reloadWorld, registry, "Reloaded");
            REQUIRE(reloaded.LoadFromFile(savePath.string()));

            CHECK(reloaded.GetName() == "Test Scene");
        }

        std::filesystem::remove(savePath);
    }

    TEST_CASE("Save and reload preserves SceneObjectIds")
    {
        auto registry = MakeTestRegistry();
        auto path = TempScenePath();
        SceneObjectId originalId;

        // Save phase
        {
            flecs::world world;
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);
            Scene scene(world, registry, "IdTest");

            auto entity = scene.CreateEntity("Tracked");
            originalId = entity.GetSceneObjectId();

            REQUIRE(scene.SaveToFile(path.string()));
        }

        // Reload into a fresh world
        {
            flecs::world reloadWorld;
            registry.RegisterComponents(reloadWorld);
            Scene::RegisterCoreECS(reloadWorld);

            Scene reloaded(reloadWorld, registry, "Reloaded");
            REQUIRE(reloaded.LoadFromFile(path.string()));

            auto found = reloaded.GetEntityByName("Tracked");
            REQUIRE(found.IsValid());
            CHECK(found.GetSceneObjectId() == originalId);
        }

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
