#include "app/Subsystem.h"
#include "maths/Maths.h"
#include "physics/PhysicsComponents.h"
#include "physics/PhysicsSubsystem.h"
#include "physics/PhysicsWorld.h"
#include "scene/Components.h"

#include <doctest/doctest.h>

#include "ecs/Flecs.h"
#include <cmath>
#include <functional>

namespace Wayfinder::Tests
{
    using namespace Wayfinder::Physics;

    /// Number of simulation steps used by gravity/movement tests (≈ 1 second at 60 Hz).
    constexpr int SIMULATION_STEPS = 60;
    constexpr float FIXED_DT = 1.0f / 60.0f;

    PhysicsBodyPose MakeBodyPose(const Float3& position, const Float3& rotationDegrees = {0.0f, 0.0f, 0.0f})
    {
        PhysicsBodyPose pose;
        pose.Position = position;
        pose.RotationDegrees = rotationDegrees;
        return pose;
    }

    /// RAII helper that binds a SubsystemCollection with PhysicsSubsystem to
    /// GameSubsystems for the lifetime of the guard.  Tests that need
    /// GameSubsystems::Find<PhysicsSubsystem>() should create one of these.
    struct PhysicsSubsystemGuard
    {
        SubsystemCollection<GameSubsystem> Subsystems;

        PhysicsSubsystemGuard()
        {
            Subsystems.Register<PhysicsSubsystem>();
            Subsystems.Initialise();
            GameSubsystems::Bind(&Subsystems);
        }

        ~PhysicsSubsystemGuard()
        {
            // Unbind before shutdown so observers that fire during
            // world teardown see a null subsystem (safe no-op).
            GameSubsystems::Unbind();
            Subsystems.Shutdown();
        }

        PhysicsWorld& GetWorld()
        {
            return Subsystems.Get<PhysicsSubsystem>()->GetWorld();
        }
    };

    /// Register the PhysicsCreateBodies observer on @p world.
    /// The observer creates a Jolt body when an entity gains the
    /// RigidBody + Collider + Transform archetype.  An optional
    /// @p onCreated callback is invoked with the new body ID.
    void RegisterCreateBodiesObserver(flecs::world& world, std::function<void(uint32_t)> onCreated = nullptr)
    {
        world.observer<RigidBodyComponent, const ColliderComponent, const TransformComponent>("PhysicsCreateBodies")
            .event(flecs::OnAdd)
            .each([onCreated = std::move(onCreated)](RigidBodyComponent& rb, const ColliderComponent& col, const TransformComponent& transform)
        {
            if (rb.RuntimeBodyId != INVALID_PHYSICS_BODY)
            {
                return;
            }
            auto* sub = GameSubsystems::Find<PhysicsSubsystem>();

            if (!sub)
            {
                return;
            }

            PhysicsBodyDescriptor desc;
            desc.Type = rb.Type;
            desc.Mass = rb.Mass;
            desc.GravityFactor = rb.GravityFactor;
            desc.LinearDamping = rb.LinearDamping;
            desc.AngularDamping = rb.AngularDamping;
            desc.LinearVelocity = rb.LinearVelocity;
            desc.AngularVelocity = rb.AngularVelocity;
            desc.Shape = col.Shape;
            desc.HalfExtents = col.HalfExtents;
            desc.Radius = col.Radius;
            desc.Height = col.Height;
            desc.Friction = col.Friction;
            desc.Restitution = col.Restitution;
            rb.RuntimeBodyId = sub->GetWorld().CreateBody(desc, MakeBodyPose(transform.Local.Position, transform.Local.RotationDegrees));

            if (onCreated)
            {
                onCreated(rb.RuntimeBodyId);
            }
        });
    }

    /// Register the PhysicsDestroyBodies observer on @p world.
    /// An optional @p onDestroyed callback fires after the body is removed.
    void RegisterDestroyBodiesObserver(flecs::world& world, std::function<void()> onDestroyed = nullptr)
    {
        world.observer<RigidBodyComponent>("PhysicsDestroyBodies")
            .event(flecs::OnRemove)
            .each([onDestroyed = std::move(onDestroyed)](flecs::entity, RigidBodyComponent& rb)
        {
            if (rb.RuntimeBodyId == INVALID_PHYSICS_BODY)
            {
                return;
            }
            auto* sub = GameSubsystems::Find<PhysicsSubsystem>();
            if (!sub)
            {
                return;
            }

            sub->GetWorld().DestroyBody(rb.RuntimeBodyId);
            rb.RuntimeBodyId = INVALID_PHYSICS_BODY;
            if (onDestroyed)
            {
                onDestroyed();
            }
        });
    }

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

            PhysicsBodyDescriptor desc;
            desc.Type = BodyType::Dynamic;
            desc.Mass = 1.0f;
            desc.Shape = ColliderShape::Box;
            desc.HalfExtents = {0.5f, 0.5f, 0.5f};

            uint32_t id = world.CreateBody(desc, MakeBodyPose(Float3{0.0f, 10.0f, 0.0f}));
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

            PhysicsBodyDescriptor desc;
            desc.Type = BodyType::Dynamic;
            desc.Shape = ColliderShape::Sphere;
            desc.Radius = 1.0f;

            uint32_t id = world.CreateBody(desc, MakeBodyPose(Float3{5.0f, 0.0f, 0.0f}));
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

            PhysicsBodyDescriptor desc;
            desc.Type = BodyType::Dynamic;
            desc.Shape = ColliderShape::Capsule;
            desc.Radius = 0.5f;
            desc.Height = 2.0f;

            uint32_t id = world.CreateBody(desc, MakeBodyPose(Float3{0.0f, 10.0f, 0.0f}));
            CHECK(id != INVALID_PHYSICS_BODY);

            Float3 pos = world.GetBodyPosition(id);
            CHECK(pos.x == doctest::Approx(0.0f));
            CHECK(pos.y == doctest::Approx(10.0f));
            CHECK(pos.z == doctest::Approx(0.0f));

            world.DestroyBody(id);
            world.Shutdown();
        }

        TEST_CASE("CreateBody applies initial rotation")
        {
            PhysicsWorld world;
            world.Initialise();

            PhysicsBodyDescriptor desc;
            desc.Type = BodyType::Static;
            desc.Shape = ColliderShape::Box;

            // 90 degrees around Y axis
            uint32_t id = world.CreateBody(desc, MakeBodyPose(Float3{0.0f, 0.0f, 0.0f}, Float3{0.0f, 90.0f, 0.0f}));
            REQUIRE(id != INVALID_PHYSICS_BODY);

            Float4 rot = world.GetBodyRotation(id);
            // For 90° Y rotation, quaternion should have a non-trivial y component.
            CHECK(std::abs(rot.y) > 0.5f);
            // w should be approximately cos(45°) ≈ 0.707
            CHECK(std::abs(rot.w) == doctest::Approx(std::cos(Maths::ToRadians(45.0f))).epsilon(0.01));

            world.DestroyBody(id);
            world.Shutdown();
        }

        // ── GetBodyRotation ─────────────────────────────────────────

        TEST_CASE("GetBodyRotation returns identity for unrotated body")
        {
            PhysicsWorld world;
            world.Initialise();

            PhysicsBodyDescriptor desc;
            desc.Type = BodyType::Static;
            desc.Shape = ColliderShape::Box;

            uint32_t id = world.CreateBody(desc, MakeBodyPose(Float3{0.0f, 0.0f, 0.0f}));
            REQUIRE(id != INVALID_PHYSICS_BODY);

            Float4 rot = world.GetBodyRotation(id);
            CHECK(rot.x == doctest::Approx(0.0f).epsilon(0.001));
            CHECK(rot.y == doctest::Approx(0.0f).epsilon(0.001));
            CHECK(rot.z == doctest::Approx(0.0f).epsilon(0.001));
            CHECK(rot.w == doctest::Approx(1.0f).epsilon(0.001));

            world.DestroyBody(id);
            world.Shutdown();
        }

        TEST_CASE("GetBodyRotation returns default for invalid body ID")
        {
            PhysicsWorld world;
            world.Initialise();

            Float4 rot = world.GetBodyRotation(INVALID_PHYSICS_BODY);
            CHECK(rot.x == doctest::Approx(0.0f));
            CHECK(rot.y == doctest::Approx(0.0f));
            CHECK(rot.z == doctest::Approx(0.0f));
            CHECK(rot.w == doctest::Approx(1.0f));

            world.Shutdown();
        }

        // ── Dynamic Body Falls Under Gravity ────────────────────────

        TEST_CASE("Dynamic body falls under gravity after stepping")
        {
            PhysicsWorld world;
            world.Initialise();

            PhysicsBodyDescriptor desc;
            desc.Type = BodyType::Dynamic;
            desc.GravityFactor = 1.0f;
            desc.Shape = ColliderShape::Box;
            desc.HalfExtents = {0.5f, 0.5f, 0.5f};

            const float startY = 10.0f;
            uint32_t id = world.CreateBody(desc, MakeBodyPose(Float3{0.0f, startY, 0.0f}));
            REQUIRE(id != INVALID_PHYSICS_BODY);

            // Step several times to let gravity take effect
            for (int i = 0; i < SIMULATION_STEPS; ++i)
            {
                world.Step(FIXED_DT);
            }

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

            PhysicsBodyDescriptor desc;
            desc.Type = BodyType::Static;
            desc.Shape = ColliderShape::Box;
            desc.HalfExtents = {5.0f, 0.5f, 5.0f};

            uint32_t id = world.CreateBody(desc, MakeBodyPose(Float3{0.0f, 0.0f, 0.0f}));
            REQUIRE(id != INVALID_PHYSICS_BODY);

            for (int i = 0; i < SIMULATION_STEPS; ++i)
            {
                world.Step(FIXED_DT);
            }

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
            PhysicsBodyDescriptor desc;
            desc.Type = BodyType::Dynamic;
            desc.GravityFactor = 1.0f;
            desc.Shape = ColliderShape::Box;

            const float startY = 20.0f;
            RigidBodyComponent rb;
            rb.RuntimeBodyId = physWorld.CreateBody(desc, MakeBodyPose(Float3{0.0f, startY, 0.0f}));
            REQUIRE(rb.RuntimeBodyId != INVALID_PHYSICS_BODY);

            // Simulate
            for (int i = 0; i < SIMULATION_STEPS; ++i)
            {
                physWorld.Step(FIXED_DT);
            }

            // Verify via position query
            Float3 pos = physWorld.GetBodyPosition(rb.RuntimeBodyId);
            CHECK(pos.y < startY);

            // Emulate what PhysicsSyncTransforms system does
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

            PhysicsBodyDescriptor dynDesc;
            dynDesc.Type = BodyType::Dynamic;
            dynDesc.Shape = ColliderShape::Box;

            PhysicsBodyDescriptor statDesc;
            statDesc.Type = BodyType::Static;
            statDesc.Shape = ColliderShape::Box;

            uint32_t dynId = world.CreateBody(dynDesc, MakeBodyPose(Float3{0.0f, 10.0f, 0.0f}));
            uint32_t statId = world.CreateBody(statDesc, MakeBodyPose(Float3{0.0f, -1.0f, 0.0f}));

            REQUIRE(dynId != INVALID_PHYSICS_BODY);
            REQUIRE(statId != INVALID_PHYSICS_BODY);
            CHECK(dynId != statId);

            for (int i = 0; i < SIMULATION_STEPS; ++i)
            {
                world.Step(FIXED_DT);
            }

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

            PhysicsBodyDescriptor desc;
            desc.Type = BodyType::Kinematic;
            desc.Shape = ColliderShape::Box;

            uint32_t id = world.CreateBody(desc, MakeBodyPose(Float3{0.0f, 0.0f, 0.0f}));
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
            // Add a small epsilon because 3.0f/60.0f can be slightly less than
            // 3 * (1.0f/60.0f) due to IEEE 754 rounding, causing the accumulator
            // to fall just short of the third tick threshold.
            int steps = world.StepFixed(3.0f / 60.0f + 0.0001f);
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

            PhysicsBodyDescriptor desc;
            desc.Type = BodyType::Dynamic;
            desc.Shape = ColliderShape::Box;

            const float startY = 10.0f;
            uint32_t id = world.CreateBody(desc, MakeBodyPose(Float3{0.0f, startY, 0.0f}));
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

        // ── Observer-based Body Creation via ECS ────────────────────

        TEST_CASE("Observer creates body when physics components are set on entity")
        {
            PhysicsSubsystemGuard guard;

            flecs::world ecsWorld;
            ecsWorld.component<RigidBodyComponent>();
            ecsWorld.component<ColliderComponent>();
            ecsWorld.component<TransformComponent>();
            ecsWorld.component<WorldTransformComponent>();

            // Register the creation observer
            RegisterCreateBodiesObserver(ecsWorld);

            // Set all components — observer should fire
            auto entity = ecsWorld.entity("TestBox");

            ecsWorld.defer_begin();
            entity.set<TransformComponent>(Float3{0.0f, 5.0f, 0.0f});
            entity.set<ColliderComponent>({});
            entity.set<RigidBodyComponent>({});
            ecsWorld.defer_end();

            // Progress once so deferred operations are flushed
            ecsWorld.progress(0.0f);

            const auto& rb = entity.get<RigidBodyComponent>();
            CHECK(rb.RuntimeBodyId != INVALID_PHYSICS_BODY);
        }

        // ── Observer-based Body Destruction ──────────────────────────

        TEST_CASE("Observer destroys body when RigidBodyComponent is removed")
        {
            PhysicsSubsystemGuard guard;

            flecs::world ecsWorld;
            ecsWorld.component<RigidBodyComponent>();
            ecsWorld.component<ColliderComponent>();
            ecsWorld.component<TransformComponent>();

            // Track body IDs created and destroyed
            uint32_t createdBodyId = INVALID_PHYSICS_BODY;
            bool bodyDestroyed = false;

            RegisterCreateBodiesObserver(ecsWorld, [&createdBodyId](uint32_t id)
            {
                createdBodyId = id;
            });

            RegisterDestroyBodiesObserver(ecsWorld, [&bodyDestroyed]()
            {
                bodyDestroyed = true;
            });

            auto entity = ecsWorld.entity("TestBox");
            ecsWorld.defer_begin();
            entity.set<TransformComponent>(Float3{0.0f, 5.0f, 0.0f});
            entity.set<ColliderComponent>({});
            entity.set<RigidBodyComponent>({});
            ecsWorld.defer_end();

            ecsWorld.progress(0.0f);

            REQUIRE(createdBodyId != INVALID_PHYSICS_BODY);
            CHECK_FALSE(bodyDestroyed);

            // Remove RigidBodyComponent — should trigger destruction
            entity.remove<RigidBodyComponent>();
            ecsWorld.progress(0.0f);

            CHECK(bodyDestroyed);
        }

        TEST_CASE("Observer destroys body when entity is deleted")
        {
            PhysicsSubsystemGuard guard;

            flecs::world ecsWorld;
            ecsWorld.component<RigidBodyComponent>();
            ecsWorld.component<ColliderComponent>();
            ecsWorld.component<TransformComponent>();

            bool bodyDestroyed = false;

            RegisterCreateBodiesObserver(ecsWorld);

            RegisterDestroyBodiesObserver(ecsWorld, [&bodyDestroyed]()
            {
                bodyDestroyed = true;
            });

            auto entity = ecsWorld.entity("TestBox");
            ecsWorld.defer_begin();
            entity.set<TransformComponent>(Float3{0.0f, 5.0f, 0.0f});
            entity.set<ColliderComponent>({});
            entity.set<RigidBodyComponent>({});
            ecsWorld.defer_end();

            ecsWorld.progress(0.0f);

            CHECK_FALSE(bodyDestroyed);

            // Delete the entire entity — should trigger destruction observer
            entity.destruct();
            ecsWorld.progress(0.0f);

            CHECK(bodyDestroyed);
        }

        // ── Full ECS Integration ────────────────────────────────────

        TEST_CASE("Full ECS pipeline: observer creates bodies, step simulates, sync writes back transforms")
        {
            PhysicsSubsystemGuard guard;

            flecs::world ecsWorld;

            // Register components
            ecsWorld.component<RigidBodyComponent>();
            ecsWorld.component<ColliderComponent>();
            ecsWorld.component<TransformComponent>();
            ecsWorld.component<WorldTransformComponent>();

            // Register creation observer
            RegisterCreateBodiesObserver(ecsWorld);

            // Register destruction observer
            RegisterDestroyBodiesObserver(ecsWorld);

            // Register step system
            ecsWorld.system("PhysicsStep")
                .kind(flecs::OnUpdate)
                .run([](flecs::iter& it)
            {
                auto* sub = GameSubsystems::Find<PhysicsSubsystem>();
                if (sub)
                {
                    sub->GetWorld().StepFixed(it.delta_time());
                }
            });

            // Register sync system
            ecsWorld.system<const RigidBodyComponent, WorldTransformComponent>("PhysicsSyncTransforms")
                .kind(flecs::OnValidate)
                .each([](flecs::entity, const RigidBodyComponent& rb, WorldTransformComponent& wt)
            {
                if (rb.RuntimeBodyId == INVALID_PHYSICS_BODY)
                {
                    return;
                }
                if (rb.Type == BodyType::Static)
                {
                    return;
                }
                auto* sub = GameSubsystems::Find<PhysicsSubsystem>();
                if (!sub)
                {
                    return;
                }
                Float3 pos = sub->GetWorld().GetBodyPosition(rb.RuntimeBodyId);
                Float4 rotQ = sub->GetWorld().GetBodyRotation(rb.RuntimeBodyId);
                wt.Position = pos;
                Quaternion q(rotQ.w, rotQ.x, rotQ.y, rotQ.z);
                Matrix4 rotMat = Maths::ToMatrix4(q);
                Matrix4 translateMat = Maths::Translate(Matrix4(1.0f), pos);
                Matrix4 scaleMat = Maths::ScaleMatrix(Matrix4(1.0f), wt.Scale);
                wt.LocalToWorld = translateMat * rotMat * scaleMat;
            });

            // Create a dynamic entity at height 20
            const float startY = 20.0f;
            auto entity = ecsWorld.entity("FallingBox");
            entity.set<TransformComponent>(Float3{0.0f, startY, 0.0f});
            entity.set<WorldTransformComponent>({});
            entity.set<RigidBodyComponent>({});
            entity.set<ColliderComponent>({});

            // Step the ECS world
            for (int i = 0; i < SIMULATION_STEPS; ++i)
            {
                ecsWorld.progress(FIXED_DT);
            }

            // Verify: WorldTransformComponent was updated by the sync system
            const auto& wt = entity.get<WorldTransformComponent>();
            CHECK(wt.Position.y < startY);

            // Verify: LocalToWorld translation matches position
            CHECK(wt.LocalToWorld[3].x == doctest::Approx(wt.Position.x).epsilon(0.001));
            CHECK(wt.LocalToWorld[3].y == doctest::Approx(wt.Position.y).epsilon(0.001));
            CHECK(wt.LocalToWorld[3].z == doctest::Approx(wt.Position.z).epsilon(0.001));

            // Verify: RigidBodyComponent got a valid runtime body ID
            const auto& rb = entity.get<RigidBodyComponent>();
            CHECK(rb.RuntimeBodyId != INVALID_PHYSICS_BODY);
        }
    }
}
