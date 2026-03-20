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

TEST_SUITE("Scene Loading")
{
    TEST_CASE("LoadFromFile loads a valid scene")
    {
        flecs::world world;
        auto registry = MakeTestRegistry();
        registry.RegisterComponents(world);
        Scene::RegisterCoreECS(world);
        Scene scene(world, registry, "Default");

        auto path = FixturesDir() / "test_scene.toml";
        bool result = scene.LoadFromFile(path.string());

        CHECK(result);
        CHECK(scene.GetName() == "Test Scene");
    }

    TEST_CASE("LoadFromFile creates expected entities")
    {
        flecs::world world;
        auto registry = MakeTestRegistry();
        registry.RegisterComponents(world);
        Scene::RegisterCoreECS(world);
        Scene scene(world, registry, "Default");

        auto path = FixturesDir() / "test_scene.toml";
        REQUIRE(scene.LoadFromFile(path.string()));

        auto camera = scene.GetEntityByName("MainCamera");
        CHECK(camera.IsValid());

        auto light = scene.GetEntityByName("PointLight");
        CHECK(light.IsValid());

        auto cube = scene.GetEntityByName("Cube");
        CHECK(cube.IsValid());
    }

    TEST_CASE("LoadFromFile applies transform components")
    {
        flecs::world world;
        auto registry = MakeTestRegistry();
        registry.RegisterComponents(world);
        Scene::RegisterCoreECS(world);
        Scene scene(world, registry, "Default");

        auto path = FixturesDir() / "test_scene.toml";
        REQUIRE(scene.LoadFromFile(path.string()));

        auto camera = scene.GetEntityByName("MainCamera");
        REQUIRE(camera.IsValid());
        REQUIRE(camera.HasComponent<TransformComponent>());

        const auto& transform = camera.GetComponent<TransformComponent>();
        CHECK(transform.Position.x == doctest::Approx(0.0f));
        CHECK(transform.Position.y == doctest::Approx(5.0f));
        CHECK(transform.Position.z == doctest::Approx(-10.0f));
    }

    TEST_CASE("LoadFromFile rebuilds hierarchy relationships")
    {
        flecs::world world;
        auto registry = MakeTestRegistry();
        registry.RegisterComponents(world);
        Scene::RegisterCoreECS(world);
        Scene scene(world, registry, "Default");

        auto path = FixturesDir() / "hierarchy_scene.toml";
        REQUIRE(scene.LoadFromFile(path.string()));

        auto parent = scene.GetEntityByName("Parent");
        auto child = scene.GetEntityByName("Child");

        CHECK(parent.IsValid());
        CHECK(child.IsValid());

        // Verify hierarchy: child should have parent as its flecs parent
        auto childHandle = child.GetHandle();
        auto parentHandle = parent.GetHandle();
        CHECK(childHandle.parent() == parentHandle);
    }

    TEST_CASE("LoadFromFile handles invalid TOML gracefully")
    {
        flecs::world world;
        auto registry = MakeTestRegistry();
        registry.RegisterComponents(world);
        Scene::RegisterCoreECS(world);
        Scene scene(world, registry, "Default");

        auto path = FixturesDir() / "bad_scene.toml";
        bool result = scene.LoadFromFile(path.string());

        CHECK_FALSE(result);
    }

    TEST_CASE("LoadFromFile handles non-existent file gracefully")
    {
        flecs::world world;
        auto registry = MakeTestRegistry();
        registry.RegisterComponents(world);
        Scene::RegisterCoreECS(world);
        Scene scene(world, registry, "Default");

        bool result = scene.LoadFromFile("nonexistent_scene.toml");
        CHECK_FALSE(result);
    }

    TEST_CASE("LoadFromFile clears previous entities")
    {
        flecs::world world;
        auto registry = MakeTestRegistry();
        registry.RegisterComponents(world);
        Scene::RegisterCoreECS(world);
        Scene scene(world, registry, "Default");

        // Create an entity before loading
        scene.CreateEntity("PreExisting");

        auto path = FixturesDir() / "minimal_scene.toml";
        REQUIRE(scene.LoadFromFile(path.string()));

        // The pre-existing entity should be gone
        auto found = scene.GetEntityByName("PreExisting");
        CHECK_FALSE(found.IsValid());
    }

    // ── SceneDocument Low-Level ─────────────────────────────

    TEST_CASE("LoadSceneDocument returns document for valid file")
    {
        auto registry = MakeTestRegistry();
        auto path = FixturesDir() / "test_scene.toml";

        auto result = LoadSceneDocument(path.string(), registry);

        CHECK(result.Document.has_value());
        CHECK(result.Errors.empty());
        CHECK(result.Document->Name == "Test Scene");
        CHECK(result.Document->Entities.size() == 3);
    }

    TEST_CASE("LoadSceneDocument returns errors for bad TOML")
    {
        auto registry = MakeTestRegistry();
        auto path = FixturesDir() / "bad_scene.toml";

        auto result = LoadSceneDocument(path.string(), registry);

        CHECK_FALSE(result.Document.has_value());
        CHECK_FALSE(result.Errors.empty());
    }

    TEST_CASE("LoadSceneDocument preserves scene settings")
    {
        auto registry = MakeTestRegistry();
        auto path = FixturesDir() / "test_scene.toml";

        auto result = LoadSceneDocument(path.string(), registry);

        REQUIRE(result.Document.has_value());
        CHECK_FALSE(result.Document->Settings.empty());
    }
}
