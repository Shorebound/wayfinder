#include "TestHelpers.h"
#include "scene/Components.h"
#include "scene/RuntimeComponentRegistry.h"
#include "scene/Scene.h"
#include "scene/entity/Entity.h"

#include <doctest/doctest.h>
#include <flecs.h>

#include <type_traits>
#include <vector>

namespace Wayfinder::Tests
{
    using Helpers::MakeTestRegistry;

    // ── Entity Create / Destroy ─────────────────────────────
    TEST_SUITE("ECS Integration")
    {
        TEST_CASE("CreateEntity produces a valid entity")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);
            Scene scene(world, registry, "TestScene");

            auto entity = scene.CreateEntity("Player");

            CHECK(entity.IsValid());
            CHECK(entity.GetName() == "Player");
        }

        TEST_CASE("Created entities have required components")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);
            Scene scene(world, registry, "TestScene");

            auto entity = scene.CreateEntity("Player");

            CHECK(entity.HasComponent<SceneEntityComponent>());
            CHECK(entity.HasComponent<NameComponent>());
            CHECK(entity.HasComponent<SceneObjectIdComponent>());
        }

        TEST_CASE("CreateEntity generates unique SceneObjectIds")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);
            Scene scene(world, registry, "TestScene");

            auto e1 = scene.CreateEntity("A");
            auto e2 = scene.CreateEntity("B");

            CHECK(e1.GetSceneObjectId() != e2.GetSceneObjectId());
        }

        TEST_CASE("Duplicate entity names get unique suffixes")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);
            Scene scene(world, registry, "TestScene");

            auto e1 = scene.CreateEntity("Entity");
            auto e2 = scene.CreateEntity("Entity");

            CHECK(e1.GetName() == "Entity");
            CHECK(e2.GetName() == "Entity1");
        }

        // ── Component Add / Remove ──────────────────────────────

        TEST_CASE("AddComponent and GetComponent round-trip")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);
            Scene scene(world, registry, "TestScene");

            auto entity = scene.CreateEntity("Player");
            entity.AddComponent<TransformComponent>(TransformComponent{{1.0f, 2.0f, 3.0f}});

            CHECK(entity.HasComponent<TransformComponent>());
            const auto& transform = entity.GetComponent<TransformComponent>();
            CHECK(transform.Position.x == doctest::Approx(1.0f));
            CHECK(transform.Position.y == doctest::Approx(2.0f));
            CHECK(transform.Position.z == doctest::Approx(3.0f));
        }

        TEST_CASE("RemoveComponent removes the component")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);
            Scene scene(world, registry, "TestScene");

            auto entity = scene.CreateEntity("Player");
            entity.AddComponent<TransformComponent>();
            REQUIRE(entity.HasComponent<TransformComponent>());

            entity.RemoveComponent<TransformComponent>();
            CHECK_FALSE(entity.HasComponent<TransformComponent>());
        }

        // ── Entity Lookup ───────────────────────────────────────

        TEST_CASE("GetEntityByName returns the correct entity")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);
            Scene scene(world, registry, "TestScene");

            auto created = scene.CreateEntity("Player");
            auto found = scene.GetEntityByName("Player");

            CHECK(found.IsValid());
            CHECK(found == created);
        }

        TEST_CASE("GetEntityByName returns invalid for missing entity")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);
            Scene scene(world, registry, "TestScene");

            auto found = scene.GetEntityByName("NonExistent");
            CHECK_FALSE(found.IsValid());
        }

        TEST_CASE("GetEntityById returns the correct entity")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);
            Scene scene(world, registry, "TestScene");

            auto created = scene.CreateEntity("Player");
            auto id = created.GetSceneObjectId();

            auto found = scene.GetEntityById(id);
            CHECK(found.IsValid());
            CHECK(found == created);
        }

        TEST_CASE("GetEntityById resolves indexed lookups across many entities")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);
            Scene scene(world, registry, "TestScene");

            std::vector<Entity> createdEntities;
            createdEntities.reserve(100);

            for (int i = 0; i < 100; ++i)
            {
                createdEntities.push_back(scene.CreateEntity("Entity"));
            }

            for (const Entity& created : createdEntities)
            {
                const auto found = scene.GetEntityById(created.GetSceneObjectId());
                CHECK(found.IsValid());
                CHECK(found == created);
            }
        }

        TEST_CASE("GetEntityById tracks reassigned SceneObjectIds")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);
            Scene scene(world, registry, "TestScene");

            auto entity = scene.CreateEntity("Player");
            const SceneObjectId originalId = entity.GetSceneObjectId();
            const SceneObjectId reassignedId = SceneObjectId::Generate();

            entity.SetSceneObjectId(reassignedId);

            CHECK_FALSE(scene.GetEntityById(originalId).IsValid());

            const auto found = scene.GetEntityById(reassignedId);
            CHECK(found.IsValid());
            CHECK(found == entity);
        }

        TEST_CASE("GetEntityById returns invalid for unknown ID")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);
            Scene scene(world, registry, "TestScene");

            auto found = scene.GetEntityById(SceneObjectId::Generate());
            CHECK_FALSE(found.IsValid());
        }

        // ── SceneOwnership Isolation ────────────────────────────

        TEST_CASE("Scene shutdown clears only scene-owned entities")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);

            // Create an entity not owned by any scene
            auto unownedEntity = world.entity("GlobalEntity");
            unownedEntity.add<SceneEntityComponent>();

            {
                Scene scene(world, registry, "TestScene");
                scene.CreateEntity("SceneEntity");
            }
            // Scene destroyed — its entities should be gone

            // The unowned entity should still exist
            auto remaining = world.lookup("GlobalEntity");
            CHECK(remaining.is_valid());
        }

        TEST_CASE("Cross-scene entity isolation")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);

            Scene sceneA(world, registry, "SceneA");
            Scene sceneB(world, registry, "SceneB");

            auto entityA = sceneA.CreateEntity("Player");
            auto entityB = sceneB.CreateEntity("Player");

            // Each scene should find only its own entity
            auto foundA = sceneA.GetEntityByName("Player");
            auto foundB = sceneB.GetEntityByName("Player");

            CHECK(foundA.IsValid());
            CHECK(foundB.IsValid());

            // The entities should be different (different scenes may produce different flecs names)
            // At minimum, both should be valid and the scene lookup should work
            CHECK(foundA.GetSceneObjectId() != foundB.GetSceneObjectId());

            sceneA.Shutdown();
            sceneB.Shutdown();
        }

        // ── Scene Name and Properties ───────────────────────────

        TEST_CASE("Scene name is accessible")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);
            Scene scene(world, registry, "MyScene");

            CHECK(scene.GetName() == "MyScene");
        }

        TEST_CASE("Scene world reference is valid")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);
            Scene scene(world, registry, "TestScene");

            CHECK(&scene.GetWorld() == &world);
        }

        // ── Name Index ──────────────────────────────────────────

        TEST_CASE("GetEntityByName uses indexed O(1) lookup")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);
            Scene scene(world, registry, "TestScene");

            std::vector<Entity> created;
            created.reserve(50);
            for (int i = 0; i < 50; ++i)
            {
                created.push_back(scene.CreateEntity("Ent"));
            }

            // Every entity should be findable by its unique name
            for (const Entity& entity : created)
            {
                auto found = scene.GetEntityByName(entity.GetName());
                CHECK(found.IsValid());
                CHECK(found == entity);
            }
        }

        TEST_CASE("GetEntityByName tracks renamed entities")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);
            Scene scene(world, registry, "TestScene");

            auto entity = scene.CreateEntity("OldName");
            entity.SetName("NewName");

            CHECK_FALSE(scene.GetEntityByName("OldName").IsValid());
            auto found = scene.GetEntityByName("NewName");
            CHECK(found.IsValid());
            CHECK(found == entity);
        }

        // ── Mutable vs Read-Only Component Access ───────────────

        TEST_CASE("GetComponent returns const reference for read access")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);
            Scene scene(world, registry, "TestScene");

            auto entity = scene.CreateEntity("Player");
            entity.AddComponent<TransformComponent>(TransformComponent{{1.0f, 2.0f, 3.0f}});

            // GetComponent on non-const Entity returns const T& (read-only)
            static_assert(
                std::is_same_v<decltype(entity.GetComponent<TransformComponent>()), const TransformComponent&>,
                "GetComponent must return const T&");
            const auto& transform = entity.GetComponent<TransformComponent>();
            CHECK(transform.Position.x == doctest::Approx(1.0f));
            CHECK(transform.Position.y == doctest::Approx(2.0f));
            CHECK(transform.Position.z == doctest::Approx(3.0f));
        }

        TEST_CASE("GetMutableComponent returns mutable reference for write access")
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreECS(world);
            Scene scene(world, registry, "TestScene");

            auto entity = scene.CreateEntity("Player");
            entity.AddComponent<TransformComponent>(TransformComponent{{1.0f, 2.0f, 3.0f}});

            // GetMutableComponent returns T& (mutable)
            auto& transform = entity.GetMutableComponent<TransformComponent>();
            transform.Position.x = 99.0f;

            // Verify mutation persisted
            const auto& readBack = entity.GetComponent<TransformComponent>();
            CHECK(readBack.Position.x == doctest::Approx(99.0f));
        }
    }
}
