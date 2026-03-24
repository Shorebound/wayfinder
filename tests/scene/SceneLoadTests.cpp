#include "TestHelpers.h"
#include "scene/Components.h"
#include "scene/RuntimeComponentRegistry.h"
#include "scene/Scene.h"
#include "scene/SceneDocument.h"
#include "scene/entity/Entity.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "ecs/Flecs.h"
#include <nlohmann/json.hpp>

namespace Wayfinder::Tests
{
    using Helpers::FixturesDir;
    using Helpers::MakeTestRegistry;

    TEST_SUITE("Scene Loading")
    {
        TEST_CASE("LoadFromFile loads a valid scene")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreComponents(world);
            Scene scene(world, registry, "Default");

            auto path = FixturesDir() / "test_scene.json";
            auto result = scene.LoadFromFile(path.string());

            CHECK(result);
            CHECK(scene.GetName() == "Test Scene");
        }

        TEST_CASE("LoadFromFile creates expected entities")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreComponents(world);
            Scene scene(world, registry, "Default");

            auto path = FixturesDir() / "test_scene.json";
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
            Scene::RegisterCoreComponents(world);
            Scene scene(world, registry, "Default");

            auto path = FixturesDir() / "test_scene.json";
            REQUIRE(scene.LoadFromFile(path.string()));

            auto camera = scene.GetEntityByName("MainCamera");
            REQUIRE(camera.IsValid());
            REQUIRE(camera.HasComponent<TransformComponent>());

            const auto& transform = camera.GetComponent<TransformComponent>();
            CHECK(transform.Local.Position.x == doctest::Approx(0.0f));
            CHECK(transform.Local.Position.y == doctest::Approx(5.0f));
            CHECK(transform.Local.Position.z == doctest::Approx(-10.0f));
        }

        TEST_CASE("LoadFromFile rebuilds hierarchy relationships")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreComponents(world);
            Scene scene(world, registry, "Default");

            auto path = FixturesDir() / "hierarchy_scene.json";
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

        TEST_CASE("LoadFromFile handles invalid JSON gracefully")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreComponents(world);
            Scene scene(world, registry, "Default");

            auto path = FixturesDir() / "bad_scene.json";
            auto result = scene.LoadFromFile(path.string());

            CHECK_FALSE(result);
        }

        TEST_CASE("Rejects scene files with duplicate object IDs")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreComponents(world);
            Scene scene(world, registry, "Default");

            auto path = FixturesDir() / "duplicate_scene_object_ids.json";
            CHECK_FALSE(scene.LoadFromFile(path.string()));
            CHECK_FALSE(scene.GetEntityByName("First").IsValid());
            CHECK_FALSE(scene.GetEntityByName("Second").IsValid());
        }

        TEST_CASE("LoadFromFile handles non-existent file gracefully")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreComponents(world);
            Scene scene(world, registry, "Default");

            auto result = scene.LoadFromFile("nonexistent_scene.json");
            CHECK_FALSE(result);
        }

        TEST_CASE("LoadFromFile clears previous entities")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreComponents(world);
            Scene scene(world, registry, "Default");

            // Create an entity before loading
            scene.CreateEntity("PreExisting");

            auto path = FixturesDir() / "minimal_scene.json";
            REQUIRE(scene.LoadFromFile(path.string()));

            // The pre-existing entity should be gone
            auto found = scene.GetEntityByName("PreExisting");
            CHECK_FALSE(found.IsValid());
        }

        // ── SceneDocument Low-Level ─────────────────────────────

        TEST_CASE("LoadSceneDocument returns document for valid file")
        {
            auto registry = MakeTestRegistry();
            auto path = FixturesDir() / "test_scene.json";

            auto result = LoadSceneDocument(path.string(), registry);

            CHECK(result.Document.has_value());
            CHECK(result.Errors.empty());
            CHECK(result.Document->Name == "Test Scene");
            CHECK(result.Document->Entities.size() == 3);
        }

        TEST_CASE("LoadSceneDocument returns errors for bad JSON")
        {
            auto registry = MakeTestRegistry();
            auto path = FixturesDir() / "bad_scene.json";

            auto result = LoadSceneDocument(path.string(), registry);

            CHECK_FALSE(result.Document.has_value());
            CHECK_FALSE(result.Errors.empty());
        }

        TEST_CASE("Reports duplicate object ID validation errors with entity context")
        {
            auto registry = MakeTestRegistry();
            auto path = FixturesDir() / "duplicate_scene_object_ids.json";
            constexpr std::string_view expectedEntityId = "550e8400-e29b-41d4-a716-446655440099";
            constexpr std::string_view expectedEntityName = "Second";

            auto result = LoadSceneDocument(path.string(), registry);

            CHECK_FALSE(result.Document.has_value());
            REQUIRE_FALSE(result.Errors.empty());

            bool foundDuplicateIdError = false;
            for (const std::string& error : result.Errors)
            {
                if (error.find("duplicate entity id") != std::string::npos && error.find(expectedEntityId) != std::string::npos && error.find(expectedEntityName) != std::string::npos)
                {
                    foundDuplicateIdError = true;
                    break;
                }
            }

            CHECK(foundDuplicateIdError);
        }

        TEST_CASE("LoadSceneDocument preserves scene settings")
        {
            auto registry = MakeTestRegistry();
            auto path = FixturesDir() / "test_scene.json";

            auto result = LoadSceneDocument(path.string(), registry);

            REQUIRE(result.Document.has_value());
            CHECK_FALSE(result.Document->Settings.empty());
        }

        TEST_CASE("LoadSceneDocument accepts version 1")
        {
            auto registry = MakeTestRegistry();
            auto path = FixturesDir() / "test_scene.json";

            auto result = LoadSceneDocument(path.string(), registry);

            REQUIRE(result.Document.has_value());
            CHECK(result.Errors.empty());
            CHECK(result.Document->Version == 1);
        }

        TEST_CASE("LoadSceneDocument rejects missing version field")
        {
            auto registry = MakeTestRegistry();
            auto tempDir = FixturesDir() / "temp";
            std::filesystem::create_directories(tempDir);
            auto path = tempDir / "no_version_scene.json";

            // Write a scene file without "version"
            {
                nlohmann::json sceneData;
                sceneData["scene_name"] = "NoVersion";
                sceneData["entities"] = nlohmann::json::array();
                std::ofstream file(path);
                file << sceneData.dump(2);
            }

            auto result = LoadSceneDocument(path.string(), registry);

            CHECK_FALSE(result.Document.has_value());
            REQUIRE_FALSE(result.Errors.empty());

            bool foundVersionError = false;
            for (const auto& error : result.Errors)
            {
                if (error.find("version") != std::string::npos)
                {
                    foundVersionError = true;
                    break;
                }
            }
            CHECK(foundVersionError);

            std::filesystem::remove(path);
        }

        TEST_CASE("SaveSceneDocument emits version 1 that round-trips correctly")
        {
            auto registry = MakeTestRegistry();
            auto tempDir = FixturesDir() / "temp";
            std::filesystem::create_directories(tempDir);
            auto path = tempDir / "save_version_scene.json";

            SceneDocument document;
            document.Name = "VersionRoundTrip";
            document.Version = 1;

            std::string error;
            REQUIRE(SaveSceneDocument(document, path.string(), error));

            // Reload and verify the version field persists
            auto result = LoadSceneDocument(path.string(), registry);
            REQUIRE(result.Document.has_value());
            CHECK(result.Errors.empty());
            CHECK(result.Document->Version == 1);

            // Also verify the raw JSON contains the version key
            {
                std::ifstream file(path);
                nlohmann::json saved = nlohmann::json::parse(file);
                REQUIRE(saved.contains("version"));
                CHECK(saved["version"].get<int>() == 1);
            }

            std::filesystem::remove(path);
        }

        TEST_CASE("LoadSceneDocument rejects wrong version number")
        {
            auto registry = MakeTestRegistry();
            auto tempDir = FixturesDir() / "temp";
            std::filesystem::create_directories(tempDir);
            auto path = tempDir / "wrong_version_scene.json";

            // Write a scene file with version 99
            {
                nlohmann::json sceneData;
                sceneData["version"] = 99;
                sceneData["scene_name"] = "WrongVersion";
                sceneData["entities"] = nlohmann::json::array();
                std::ofstream file(path);
                file << sceneData.dump(2);
            }

            auto result = LoadSceneDocument(path.string(), registry);

            CHECK_FALSE(result.Document.has_value());
            REQUIRE_FALSE(result.Errors.empty());

            bool foundVersionError = false;
            for (const auto& error : result.Errors)
            {
                if (error.find("Unsupported scene format version") != std::string::npos)
                {
                    foundVersionError = true;
                    break;
                }
            }
            CHECK(foundVersionError);

            std::filesystem::remove(path);
        }
    }
}
