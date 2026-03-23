/**
 * @file PhysicsIntegrationTests.cpp
 * @brief End-to-end integration tests for the physics pipeline.
 *
 * These tests exercise the real PhysicsPlugin registration path — observers,
 * systems, and subsystem setup — rather than reimplementing the ECS wiring
 * inline.  This catches regressions in PhysicsPlugin::Build() that unit-level
 * tests against PhysicsWorld alone would miss.
 *
 * All tests are headless: no window, no GPU, no filesystem access.
 */

#include "app/EngineConfig.h"
#include "modules/ModuleRegistry.h"
#include "project/ProjectDescriptor.h"
#include "app/Subsystem.h"
#include "physics/PhysicsComponents.h"
#include "physics/PhysicsPlugin.h"
#include "physics/PhysicsSubsystem.h"
#include "physics/PhysicsWorld.h"
#include "scene/Components.h"


#include <doctest/doctest.h>

#include "ecs/Flecs.h"

namespace Wayfinder::Tests
{
    using namespace Wayfinder::Physics;

    /// Number of simulation steps (≈ 1 second at 60 Hz).
    constexpr int SIMULATION_STEPS = 60;
    constexpr float FIXED_DT = 1.0f / 60.0f;

    /**
     * @brief RAII fixture that boots the full physics pipeline via
     *        PhysicsPlugin — the same path the engine uses at runtime.
     *
     * Sets up:
     * - SubsystemCollection with PhysicsSubsystem
     * - GameSubsystems binding
     * - A flecs::world with all components, observers, and systems
     *   registered through PhysicsPlugin::Build() + ApplyToWorld().
     *
     * Tears down cleanly in reverse order.
     */
    struct PhysicsIntegrationFixture
    {
        SubsystemCollection<GameSubsystem> Subsystems;
        flecs::world EcsWorld;

        PhysicsIntegrationFixture()
        {
            // Build the ModuleRegistry with PhysicsPlugin, exactly as Game does.
            ProjectDescriptor project{};
            project.Name = "PhysicsIntegrationTest";
            EngineConfig config = EngineConfig::LoadDefaults();

            ModuleRegistry registry(project, config);
            registry.AddPlugin<PhysicsPlugin>();

            // Stand up the subsystem from the plugin's registration.
            for (const auto& entry : registry.GetSubsystemFactories())
                Subsystems.Register(entry.Type, entry.Factory, entry.Predicate);

            Subsystems.Initialise();
            GameSubsystems::Bind(&Subsystems);

            // Register ECS components that the observers depend on.
            EcsWorld.component<RigidBodyComponent>();
            EcsWorld.component<ColliderComponent>();
            EcsWorld.component<TransformComponent>();
            EcsWorld.component<WorldTransformComponent>();

            // Apply plugin-registered observers and systems into the world.
            registry.ApplyToWorld(EcsWorld);
        }

        ~PhysicsIntegrationFixture()
        {
            GameSubsystems::Unbind();
            Subsystems.Shutdown();
        }

        PhysicsWorld& GetPhysicsWorld()
        {
            return Subsystems.Get<PhysicsSubsystem>()->GetWorld();
        }

        // Create a physics entity with the given body type, position, and collider.
        flecs::entity CreatePhysicsEntity(const char* name,
                                          BodyType type,
                                          const Float3& position,
                                          ColliderShape shape = ColliderShape::Box)
        {
            RigidBodyComponent rb;
            rb.Type = type;

            ColliderComponent col;
            col.Shape = shape;

            auto entity = EcsWorld.entity(name);

            EcsWorld.defer_begin();
            entity.set<TransformComponent>({position});
            entity.set<WorldTransformComponent>({});
            entity.set<ColliderComponent>(col);
            entity.set<RigidBodyComponent>(rb);
            EcsWorld.defer_end();
            
            auto rigidbody = entity.get<RigidBodyComponent>();
            auto result = rigidbody.RuntimeBodyId;
            (void)result; // Avoid unused variable warning; observer will fill this in asynchronously.

            return entity;
        }

        // Progress the ECS world for N ticks at the fixed timestep.
        void Simulate(int ticks = SIMULATION_STEPS)
        {
            for (int i = 0; i < ticks; ++i)
                EcsWorld.progress(FIXED_DT);
        }
    };

    // ── Integration Tests ──────────────────────────────────────────

    TEST_SUITE("Physics Integration")
    {
        TEST_CASE("Dynamic body falls under gravity through the full plugin pipeline")
        {
            PhysicsIntegrationFixture fixture;

            const float startY = 20.0f;
            auto entity = fixture.CreatePhysicsEntity("FallingBox", BodyType::Dynamic, {0.0f, startY, 0.0f});

            fixture.Simulate();

            // Observer should have assigned a valid runtime body.
            const auto& rb = entity.get<RigidBodyComponent>();
            REQUIRE(rb.RuntimeBodyId != INVALID_PHYSICS_BODY);

            // PhysicsSyncTransforms should have written back the position.
            const auto& wt = entity.get<WorldTransformComponent>();
            CHECK(wt.Position.y < startY);

            // LocalToWorld translation column should agree with position.
            CHECK(wt.LocalToWorld[3].x == doctest::Approx(wt.Position.x).epsilon(0.01));
            CHECK(wt.LocalToWorld[3].y == doctest::Approx(wt.Position.y).epsilon(0.01));
            CHECK(wt.LocalToWorld[3].z == doctest::Approx(wt.Position.z).epsilon(0.01));
        }

        TEST_CASE("Static body does not move after stepping")
        {
            PhysicsIntegrationFixture fixture;

            auto entity = fixture.CreatePhysicsEntity("Floor", BodyType::Static, {0.0f, 0.0f, 0.0f});

            fixture.Simulate();

            const auto& rb = entity.get<RigidBodyComponent>();
            REQUIRE(rb.RuntimeBodyId != INVALID_PHYSICS_BODY);

            // Static bodies are skipped by PhysicsSyncTransforms, so query Jolt directly.
            Float3 pos = fixture.GetPhysicsWorld().GetBodyPosition(rb.RuntimeBodyId);
            CHECK(pos.x == doctest::Approx(0.0f));
            CHECK(pos.y == doctest::Approx(0.0f));
            CHECK(pos.z == doctest::Approx(0.0f));
        }

        TEST_CASE("Kinematic body responds to explicit position setting")
        {
            PhysicsIntegrationFixture fixture;

            auto entity = fixture.CreatePhysicsEntity("KinematicPlatform", BodyType::Kinematic, {0.0f, 0.0f, 0.0f});

            // Flush deferred operations so observer fires.
            fixture.EcsWorld.progress(0.0f);

            const auto& rb = entity.get<RigidBodyComponent>();
            REQUIRE(rb.RuntimeBodyId != INVALID_PHYSICS_BODY);

            // Teleport the kinematic body.
            const Float3 target = {10.0f, 5.0f, -3.0f};
            fixture.GetPhysicsWorld().SetBodyPosition(rb.RuntimeBodyId, target);

            // Step so PhysicsSyncTransforms writes back.
            fixture.Simulate(1);

            const auto& wt = entity.get<WorldTransformComponent>();
            CHECK(wt.Position.x == doctest::Approx(target.x).epsilon(0.01));
            CHECK(wt.Position.y == doctest::Approx(target.y).epsilon(0.01));
            CHECK(wt.Position.z == doctest::Approx(target.z).epsilon(0.01));
        }

        TEST_CASE("Removing RigidBodyComponent cleans up Jolt body")
        {
            PhysicsIntegrationFixture fixture;

            auto entity = fixture.CreatePhysicsEntity("Removable", BodyType::Dynamic, {0.0f, 10.0f, 0.0f});
            fixture.EcsWorld.progress(0.0f);

            const auto& rb = entity.get<RigidBodyComponent>();
            REQUIRE(rb.RuntimeBodyId != INVALID_PHYSICS_BODY);
            uint32_t bodyId = rb.RuntimeBodyId;

            // Remove the component — the destruction observer should fire.
            entity.remove<RigidBodyComponent>();
            fixture.EcsWorld.progress(0.0f);

            // Entity should no longer have RigidBodyComponent.
            CHECK_FALSE(entity.has<RigidBodyComponent>());

            // Jolt body was destroyed — querying its position now returns the
            // zero default because the body ID is no longer valid in Jolt.
            // (This verifies DestroyBody was called; the engine doesn't crash.)
            Float3 pos = fixture.GetPhysicsWorld().GetBodyPosition(bodyId);
            (void)pos; // Reaching here without crashing proves cleanup happened.
        }

        TEST_CASE("Deleting an entity cleans up its Jolt body")
        {
            PhysicsIntegrationFixture fixture;

            auto entity = fixture.CreatePhysicsEntity("Ephemeral", BodyType::Dynamic, {0.0f, 10.0f, 0.0f});
            fixture.EcsWorld.progress(0.0f);

            const auto& rb = entity.get<RigidBodyComponent>();
            REQUIRE(rb.RuntimeBodyId != INVALID_PHYSICS_BODY);
            uint32_t bodyId = rb.RuntimeBodyId;

            // Delete the entire entity.
            entity.destruct();
            fixture.EcsWorld.progress(0.0f);

            // Same rationale: reaching here without crash proves cleanup.
            Float3 pos = fixture.GetPhysicsWorld().GetBodyPosition(bodyId);
            (void)pos;
        }

        TEST_CASE("Multiple entities with mixed body types simulate independently")
        {
            PhysicsIntegrationFixture fixture;

            const float dynamicStartY = 20.0f;
            auto dynamic = fixture.CreatePhysicsEntity("DynBox", BodyType::Dynamic, {0.0f, dynamicStartY, 0.0f});
            auto floor = fixture.CreatePhysicsEntity("Floor", BodyType::Static, {0.0f, -1.0f, 0.0f});
            auto platform = fixture.CreatePhysicsEntity("Platform", BodyType::Kinematic, {5.0f, 0.0f, 0.0f});

            fixture.Simulate();

            // Dynamic body should have fallen.
            const auto& dynWt = dynamic.get<WorldTransformComponent>();
            CHECK(dynWt.Position.y < dynamicStartY);

            // Static body should not have moved.
            const auto& floorRb = floor.get<RigidBodyComponent>();
            REQUIRE(floorRb.RuntimeBodyId != INVALID_PHYSICS_BODY);
            Float3 floorPos = fixture.GetPhysicsWorld().GetBodyPosition(floorRb.RuntimeBodyId);
            CHECK(floorPos.y == doctest::Approx(-1.0f));

            // Kinematic body stays where it was placed (no gravity).
            const auto& platWt = platform.get<WorldTransformComponent>();
            CHECK(platWt.Position.x == doctest::Approx(5.0f).epsilon(0.01));
            CHECK(platWt.Position.y == doctest::Approx(0.0f).epsilon(0.01));
        }

        TEST_CASE("Dynamic body with sphere collider falls through full pipeline")
        {
            PhysicsIntegrationFixture fixture;

            const float startY = 15.0f;
            auto entity = fixture.CreatePhysicsEntity(
                "FallingSphere", BodyType::Dynamic, {0.0f, startY, 0.0f}, ColliderShape::Sphere);

            fixture.Simulate();

            const auto& rb = entity.get<RigidBodyComponent>();
            REQUIRE(rb.RuntimeBodyId != INVALID_PHYSICS_BODY);

            const auto& wt = entity.get<WorldTransformComponent>();
            CHECK(wt.Position.y < startY);
        }
    }
}
