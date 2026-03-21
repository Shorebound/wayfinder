#include "physics/PhysicsComponents.h"
#include "physics/PhysicsSubsystem.h"
#include "physics/PhysicsWorld.h"
#include "scene/Components.h"
#include "core/Subsystem.h"

#include <doctest/doctest.h>

#include <flecs.h>
#include <cmath>

using namespace Wayfinder;

namespace
{
    /// Number of simulation steps used by gravity/movement tests (≈ 1 second at 60 Hz).
    constexpr int SIMULATION_STEPS = 60;
    constexpr float FIXED_DT = 1.0f / 60.0f;
} // anonymous namespace

// ── PhysicsWorld Lifecycle ──────────────────────────────────

TEST_SUITE("Physics")
{
    TEST_CASE("PhysicsWorld initialises and shuts down cleanly")
    {
        PhysicsWorld world;
        CHECK_FALSE(world.IsInitialised());

        world.Initialise();
        CHECK(world.IsInitialised());

        world.Shutdown();
        CHECK_FALSE(world.IsInitialised());
    }

    TEST_CASE("PhysicsSubsystem registered via SubsystemCollection")
    {
        SubsystemCollection<GameSubsystem> collection;
        collection.Register<PhysicsSubsystem>();
        collection.Initialise();

        auto* subsystem = collection.Get<PhysicsSubsystem>();
        REQUIRE(subsystem != nullptr);
        CHECK(subsystem->GetWorld().IsInitialised());

        collection.Shutdown();

        // After shutdown, Get returns nullptr (subsystem is destroyed).
        CHECK(collection.Get<PhysicsSubsystem>() == nullptr);
    }

    // ── Component Defaults ──────────────────────────────────────

    TEST_CASE("RigidBodyComponent has sensible defaults")
    {
        RigidBodyComponent rb;
        CHECK(rb.Type == BodyType::Dynamic);
        CHECK(rb.Mass == doctest::Approx(1.0f));
        CHECK(rb.GravityFactor == doctest::Approx(1.0f));
        CHECK(rb.LinearDamping == doctest::Approx(0.05f));
        CHECK(rb.AngularDamping == doctest::Approx(0.05f));
        CHECK(rb.RuntimeBodyId == INVALID_PHYSICS_BODY);
    }

    TEST_CASE("ColliderComponent has sensible defaults")
    {
        ColliderComponent col;
        CHECK(col.Shape == ColliderShape::Box);
        CHECK(col.HalfExtents.x == doctest::Approx(0.5f));
        CHECK(col.HalfExtents.y == doctest::Approx(0.5f));
        CHECK(col.HalfExtents.z == doctest::Approx(0.5f));
        CHECK(col.Radius == doctest::Approx(0.5f));
        CHECK(col.Height == doctest::Approx(1.0f));
        CHECK(col.Friction == doctest::Approx(0.2f));
        CHECK(col.Restitution == doctest::Approx(0.0f));
    }

    // ── Body Creation ───────────────────────────────────────────

    TEST_CASE("CreateBody returns a valid body ID for a dynamic box")
    {
        PhysicsWorld world;
        world.Initialise();

        RigidBodyComponent rb;
        rb.Type = BodyType::Dynamic;
        rb.Mass = 1.0f;

        ColliderComponent col;
        col.Shape = ColliderShape::Box;
        col.HalfExtents = {0.5f, 0.5f, 0.5f};

        uint32_t id = world.CreateBody(rb, col, {0.0f, 10.0f, 0.0f});
        CHECK(id != INVALID_PHYSICS_BODY);

        Float3 pos = world.GetBodyPosition(id);
        CHECK(pos.x == doctest::Approx(0.0f));
        CHECK(pos.y == doctest::Approx(10.0f));
        CHECK(pos.z == doctest::Approx(0.0f));

        world.DestroyBody(id);
        world.Shutdown();
    }

    TEST_CASE("CreateBody works with sphere collider")
    {
        PhysicsWorld world;
        world.Initialise();

        RigidBodyComponent rb;
        rb.Type = BodyType::Dynamic;

        ColliderComponent col;
        col.Shape = ColliderShape::Sphere;
        col.Radius = 1.0f;

        uint32_t id = world.CreateBody(rb, col, {5.0f, 0.0f, 0.0f});
        CHECK(id != INVALID_PHYSICS_BODY);

        Float3 pos = world.GetBodyPosition(id);
        CHECK(pos.x == doctest::Approx(5.0f));

        world.DestroyBody(id);
        world.Shutdown();
    }

    TEST_CASE("CreateBody works with capsule collider")
    {
        PhysicsWorld world;
        world.Initialise();

        RigidBodyComponent rb;
        rb.Type = BodyType::Dynamic;

        ColliderComponent col;
        col.Shape = ColliderShape::Capsule;
        col.Radius = 0.5f;
        col.Height = 2.0f;

        uint32_t id = world.CreateBody(rb, col, {0.0f, 10.0f, 0.0f});
        CHECK(id != INVALID_PHYSICS_BODY);

        Float3 pos = world.GetBodyPosition(id);
        CHECK(pos.x == doctest::Approx(0.0f));
        CHECK(pos.y == doctest::Approx(10.0f));
        CHECK(pos.z == doctest::Approx(0.0f));

        world.DestroyBody(id);
        world.Shutdown();
    }

    // ── Dynamic Body Falls Under Gravity ────────────────────────

    TEST_CASE("Dynamic body falls under gravity after stepping")
    {
        PhysicsWorld world;
        world.Initialise();

        RigidBodyComponent rb;
        rb.Type = BodyType::Dynamic;
        rb.GravityFactor = 1.0f;

        ColliderComponent col;
        col.Shape = ColliderShape::Box;
        col.HalfExtents = {0.5f, 0.5f, 0.5f};

        const float startY = 10.0f;
        uint32_t id = world.CreateBody(rb, col, {0.0f, startY, 0.0f});
        REQUIRE(id != INVALID_PHYSICS_BODY);

        // Step several times to let gravity take effect
        for (int i = 0; i < SIMULATION_STEPS; ++i)
            world.Step(FIXED_DT);

        Float3 pos = world.GetBodyPosition(id);
        CHECK(pos.y < startY);

        world.DestroyBody(id);
        world.Shutdown();
    }

    // ── Static Body Does Not Move ───────────────────────────────

    TEST_CASE("Static body does not move after stepping")
    {
        PhysicsWorld world;
        world.Initialise();

        RigidBodyComponent rb;
        rb.Type = BodyType::Static;

        ColliderComponent col;
        col.Shape = ColliderShape::Box;
        col.HalfExtents = {5.0f, 0.5f, 5.0f};

        uint32_t id = world.CreateBody(rb, col, {0.0f, 0.0f, 0.0f});
        REQUIRE(id != INVALID_PHYSICS_BODY);

        for (int i = 0; i < SIMULATION_STEPS; ++i)
            world.Step(FIXED_DT);

        Float3 pos = world.GetBodyPosition(id);
        CHECK(pos.x == doctest::Approx(0.0f));
        CHECK(pos.y == doctest::Approx(0.0f));
        CHECK(pos.z == doctest::Approx(0.0f));

        world.DestroyBody(id);
        world.Shutdown();
    }

    // ── Transform Writeback via ECS ─────────────────────────────

    TEST_CASE("Physics writeback updates WorldTransformComponent")
    {
        PhysicsWorld physWorld;
        physWorld.Initialise();

        // Create a body high up so it falls
        RigidBodyComponent rb;
        rb.Type = BodyType::Dynamic;
        rb.GravityFactor = 1.0f;

        ColliderComponent col;
        col.Shape = ColliderShape::Box;

        const float startY = 20.0f;
        rb.RuntimeBodyId = physWorld.CreateBody(rb, col, {0.0f, startY, 0.0f});
        REQUIRE(rb.RuntimeBodyId != INVALID_PHYSICS_BODY);

        // Simulate
        for (int i = 0; i < SIMULATION_STEPS; ++i)
            physWorld.Step(FIXED_DT);

        // Verify via position query
        Float3 pos = physWorld.GetBodyPosition(rb.RuntimeBodyId);
        CHECK(pos.y < startY);

        // Emulate what PhysicsWriteback system does
        WorldTransformComponent wt;
        wt.Position = pos;
        wt.LocalToWorld[3] = Float4(pos, 1.0f);

        CHECK(wt.Position.y < startY);
        CHECK(wt.LocalToWorld[3].y < startY);

        physWorld.DestroyBody(rb.RuntimeBodyId);
        physWorld.Shutdown();
    }

    // ── Multiple Bodies ─────────────────────────────────────────

    TEST_CASE("Multiple bodies are independent")
    {
        PhysicsWorld world;
        world.Initialise();

        ColliderComponent col;
        col.Shape = ColliderShape::Box;

        RigidBodyComponent dynamic;
        dynamic.Type = BodyType::Dynamic;

        RigidBodyComponent staticBody;
        staticBody.Type = BodyType::Static;

        uint32_t dynId = world.CreateBody(dynamic, col, {0.0f, 10.0f, 0.0f});
        uint32_t statId = world.CreateBody(staticBody, col, {0.0f, -1.0f, 0.0f});

        REQUIRE(dynId != INVALID_PHYSICS_BODY);
        REQUIRE(statId != INVALID_PHYSICS_BODY);
        CHECK(dynId != statId);

        for (int i = 0; i < SIMULATION_STEPS; ++i)
            world.Step(FIXED_DT);

        // Dynamic body fell
        Float3 dynPos = world.GetBodyPosition(dynId);
        CHECK(dynPos.y < 10.0f);

        // Static body stayed
        Float3 statPos = world.GetBodyPosition(statId);
        CHECK(statPos.y == doctest::Approx(-1.0f));

        world.DestroyBody(dynId);
        world.DestroyBody(statId);
        world.Shutdown();
    }

    // ── DestroyBody is safe for invalid ID ──────────────────────

    TEST_CASE("DestroyBody handles invalid ID gracefully")
    {
        PhysicsWorld world;
        world.Initialise();

        // Should not crash
        world.DestroyBody(INVALID_PHYSICS_BODY);

        world.Shutdown();
    }

    // ── SetBodyPosition for kinematic bodies ────────────────────

    TEST_CASE("SetBodyPosition updates kinematic body position")
    {
        PhysicsWorld world;
        world.Initialise();

        RigidBodyComponent rb;
        rb.Type = BodyType::Kinematic;

        ColliderComponent col;
        col.Shape = ColliderShape::Box;

        uint32_t id = world.CreateBody(rb, col, {0.0f, 0.0f, 0.0f});
        REQUIRE(id != INVALID_PHYSICS_BODY);

        world.SetBodyPosition(id, {5.0f, 10.0f, 15.0f});

        Float3 pos = world.GetBodyPosition(id);
        CHECK(pos.x == doctest::Approx(5.0f));
        CHECK(pos.y == doctest::Approx(10.0f));
        CHECK(pos.z == doctest::Approx(15.0f));

        world.DestroyBody(id);
        world.Shutdown();
    }

    // ── Fixed Timestep ──────────────────────────────────────────

    TEST_CASE("Default fixed timestep is 1/60")
    {
        PhysicsWorld world;
        CHECK(world.GetFixedTimestep() == doctest::Approx(1.0f / 60.0f));
    }

    TEST_CASE("SetFixedTimestep changes the timestep")
    {
        PhysicsWorld world;
        world.SetFixedTimestep(1.0f / 120.0f);
        CHECK(world.GetFixedTimestep() == doctest::Approx(1.0f / 120.0f));
    }

    TEST_CASE("SetFixedTimestep rejects non-positive values")
    {
        PhysicsWorld world;
        float original = world.GetFixedTimestep();

        world.SetFixedTimestep(0.0f);
        CHECK(world.GetFixedTimestep() == doctest::Approx(original));

        world.SetFixedTimestep(-1.0f);
        CHECK(world.GetFixedTimestep() == doctest::Approx(original));
    }

    TEST_CASE("StepFixed performs no steps when frame time is below fixed timestep")
    {
        PhysicsWorld world;
        world.Initialise();
        world.SetFixedTimestep(1.0f / 60.0f);

        // A very short frame (half of one tick) should not step yet.
        int steps = world.StepFixed(0.5f / 60.0f);
        CHECK(steps == 0);

        world.Shutdown();
    }

    TEST_CASE("StepFixed performs exactly one step for one tick of frame time")
    {
        PhysicsWorld world;
        world.Initialise();
        world.SetFixedTimestep(1.0f / 60.0f);

        int steps = world.StepFixed(1.0f / 60.0f);
        CHECK(steps == 1);

        world.Shutdown();
    }

    TEST_CASE("StepFixed accumulates across multiple calls")
    {
        PhysicsWorld world;
        world.Initialise();
        world.SetFixedTimestep(1.0f / 60.0f);

        // Two half-tick frames should produce one step total.
        int steps1 = world.StepFixed(0.5f / 60.0f);
        CHECK(steps1 == 0);

        int steps2 = world.StepFixed(0.5f / 60.0f);
        CHECK(steps2 == 1);

        world.Shutdown();
    }

    TEST_CASE("StepFixed performs multiple steps for large frame times")
    {
        PhysicsWorld world;
        world.Initialise();
        world.SetFixedTimestep(1.0f / 60.0f);

        // Three ticks in one frame should produce three steps.
        int steps = world.StepFixed(3.0f / 60.0f);
        CHECK(steps == 3);

        world.Shutdown();
    }

    TEST_CASE("StepFixed caps accumulated time to prevent spiral of death")
    {
        PhysicsWorld world;
        world.Initialise();
        world.SetFixedTimestep(1.0f / 60.0f);

        // A huge frame time (e.g. debugger pause) should be capped.
        // The cap is 0.25s, so at 1/60 ≈ 15 steps max.
        int steps = world.StepFixed(10.0f);
        CHECK(steps <= 15);
        CHECK(steps > 0);

        world.Shutdown();
    }

    TEST_CASE("StepFixed advances simulation with fixed timestep")
    {
        PhysicsWorld world;
        world.Initialise();
        world.SetFixedTimestep(FIXED_DT);

        RigidBodyComponent rb;
        rb.Type = BodyType::Dynamic;

        ColliderComponent col;
        col.Shape = ColliderShape::Box;

        const float startY = 10.0f;
        uint32_t id = world.CreateBody(rb, col, {0.0f, startY, 0.0f});
        REQUIRE(id != INVALID_PHYSICS_BODY);

        // Simulate ~1 second via StepFixed with variable frame times
        float totalTime = 0.0f;
        while (totalTime < 1.0f)
        {
            world.StepFixed(1.0f / 30.0f); // 30 fps frames
            totalTime += 1.0f / 30.0f;
        }

        Float3 pos = world.GetBodyPosition(id);
        CHECK(pos.y < startY);

        world.DestroyBody(id);
        world.Shutdown();
    }

    // ── Full ECS integration via flecs world ────────────────────

    TEST_CASE("ECS systems create bodies and write back positions")
    {
        // Stand up a PhysicsSubsystem so GameSubsystems::Find works.
        SubsystemCollection<GameSubsystem> subsystems;
        subsystems.Register<PhysicsSubsystem>();
        subsystems.Initialise();
        GameSubsystems::Bind(&subsystems);

        flecs::world ecsWorld;

        // Register components
        ecsWorld.component<RigidBodyComponent>();
        ecsWorld.component<ColliderComponent>();
        ecsWorld.component<TransformComponent>();
        ecsWorld.component<WorldTransformComponent>();

        // Register the PhysicsSync system (creates Jolt bodies)
        ecsWorld.system<RigidBodyComponent, const ColliderComponent, const TransformComponent>("PhysicsSync")
            .kind(flecs::PreUpdate)
            .each([](flecs::entity e,
                     RigidBodyComponent& rb,
                     const ColliderComponent& col,
                     const TransformComponent& transform)
            {
                if (rb.RuntimeBodyId != INVALID_PHYSICS_BODY)
                    return;
                auto* sub = GameSubsystems::Find<PhysicsSubsystem>();
                if (!sub) return;
                rb.RuntimeBodyId = sub->GetWorld().CreateBody(rb, col, transform.Position);
            });

        // Register the PhysicsStep system (steps the world via fixed timestep)
        ecsWorld.system("PhysicsStep")
            .kind(flecs::OnUpdate)
            .iter([](flecs::iter& it)
            {
                auto* sub = GameSubsystems::Find<PhysicsSubsystem>();
                if (sub) sub->GetWorld().StepFixed(it.delta_time());
            });

        // Register the PhysicsWriteback system
        ecsWorld.system<const RigidBodyComponent, WorldTransformComponent>("PhysicsWriteback")
            .kind(flecs::OnValidate)
            .each([](flecs::entity e,
                     const RigidBodyComponent& rb,
                     WorldTransformComponent& wt)
            {
                if (rb.RuntimeBodyId == INVALID_PHYSICS_BODY) return;
                if (rb.Type == BodyType::Static) return;
                auto* sub = GameSubsystems::Find<PhysicsSubsystem>();
                if (!sub) return;
                wt.Position = sub->GetWorld().GetBodyPosition(rb.RuntimeBodyId);
            });

        // Create a dynamic entity at height 20
        const float startY = 20.0f;
        auto entity = ecsWorld.entity("FallingBox");
        entity.set<TransformComponent>({{0.0f, startY, 0.0f}});
        entity.set<WorldTransformComponent>({});
        entity.set<RigidBodyComponent>({});
        entity.set<ColliderComponent>({});

        // Step the ECS world (which runs PhysicsSync → PhysicsStep → PhysicsWriteback)
        for (int i = 0; i < SIMULATION_STEPS; ++i)
            ecsWorld.progress(FIXED_DT);

        // Verify: the WorldTransformComponent was updated by the writeback system
        const auto* wt = entity.get<WorldTransformComponent>();
        REQUIRE(wt != nullptr);
        CHECK(wt->Position.y < startY);

        // Verify: the RigidBodyComponent got a valid runtime body ID
        const auto* rb = entity.get<RigidBodyComponent>();
        REQUIRE(rb != nullptr);
        CHECK(rb->RuntimeBodyId != INVALID_PHYSICS_BODY);

        GameSubsystems::Unbind();
        subsystems.Shutdown();
    }
}
