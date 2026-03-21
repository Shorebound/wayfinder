#include "physics/PhysicsComponents.h"
#include "physics/PhysicsSubsystem.h"
#include "physics/PhysicsWorld.h"
#include "scene/Components.h"
#include "core/Subsystem.h"

#include <doctest/doctest.h>

#include <flecs.h>
#include <cmath>

using namespace Wayfinder;

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
        CHECK_FALSE(subsystem->GetWorld().IsInitialised());
    }

    // ── Component Defaults ──────────────────────────────────────

    TEST_CASE("RigidBodyComponent has sensible defaults")
    {
        RigidBodyComponent rb;
        CHECK(rb.Type == BodyType::Dynamic);
        CHECK(rb.Mass == doctest::Approx(1.0f));
        CHECK(rb.GravityFactor == doctest::Approx(1.0f));
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
        for (int i = 0; i < 60; ++i)
            world.Step(1.0f / 60.0f);

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

        for (int i = 0; i < 60; ++i)
            world.Step(1.0f / 60.0f);

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
        for (int i = 0; i < 60; ++i)
            physWorld.Step(1.0f / 60.0f);

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

        for (int i = 0; i < 60; ++i)
            world.Step(1.0f / 60.0f);

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
}
