#pragma once

#include "PhysicsComponents.h"
#include "wayfinder_exports.h"

#include <cstdint>
#include <memory>

// Forward-declare Jolt types to keep the header Jolt-free.
namespace JPH
{
    class PhysicsSystem;
    class TempAllocator;
    class JobSystem;
    class BroadPhaseLayerInterface;
    class ObjectVsBroadPhaseLayerFilter;
    class ObjectLayerPairFilter;
} // namespace JPH

namespace Wayfinder::Physics
{
    /**
     * @brief Physics-native description of a body to create.
     *
     * Decouples PhysicsWorld from ECS component types.  The ECS observer
     * translates RigidBodyComponent + ColliderComponent into this struct
     * before calling CreateBody().
     */
    struct PhysicsBodyDescriptor
    {
        BodyType Type = BodyType::Dynamic;
        ColliderShape Shape = ColliderShape::Box;

        // Mass & dynamics
        /// Explicit mass override for dynamic bodies.  Any positive value
        /// (including 1.0f) is used as-is.  A value <= 0 means "use shape-
        /// computed mass" (Jolt default).
        float Mass = 1.0f;
        float GravityFactor = 1.0f;
        float LinearDamping = 0.05f;
        float AngularDamping = 0.05f;
        Float3 LinearVelocity = {0.0f, 0.0f, 0.0f};
        Float3 AngularVelocity = {0.0f, 0.0f, 0.0f};

        // Shape parameters
        Float3 HalfExtents = {0.5f, 0.5f, 0.5f};
        float Radius = 0.5f;
        float Height = 1.0f;

        // Material
        float Friction = 0.2f;
        float Restitution = 0.0f;
    };

    /**
     * @brief Thin wrapper around Jolt's PhysicsSystem.
     *
     * Owns the Jolt world, temp allocator, job system and layer callbacks.
     * The engine creates exactly one instance via PhysicsSubsystem.
     *
     * Simulation uses a fixed timestep accumulator: call StepFixed() each
     * frame and the world advances in uniform increments of GetFixedTimestep().
     */
    class WAYFINDER_API PhysicsWorld
    {
    public:
        PhysicsWorld();
        ~PhysicsWorld();

        // Non-copyable, non-movable (owns Jolt runtime state).
        PhysicsWorld(const PhysicsWorld&) = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;
        PhysicsWorld(PhysicsWorld&&) = delete;
        PhysicsWorld& operator=(PhysicsWorld&&) = delete;

        /// Initialise Jolt runtime, allocate the physics system.
        void Initialise();

        /// Tear down all bodies and free Jolt state.
        void Shutdown();

        /// Step the simulation by exactly @p deltaTime seconds (single step).
        /// Prefer StepFixed() for frame-driven updates.
        void Step(float deltaTime);

        /// Accumulate @p frameDeltaTime and advance the simulation in fixed
        /// increments of GetFixedTimestep().  Returns the number of sub-steps
        /// performed (0 if the accumulated time is still below one tick).
        int StepFixed(float frameDeltaTime);

        /// Set the fixed simulation timestep (in seconds).
        /// Must be positive. Takes effect on the next StepFixed() call.
        void SetFixedTimestep(float timestep);

        /// @return The current fixed timestep in seconds.
        float GetFixedTimestep() const { return m_fixedTimestep; }

        /// Create a Jolt body from a physics-native descriptor and return its
        /// raw BodyID value.
        /// @p rotationDegrees is applied as Euler ZYX (matching ComposeTransform).
        /// Returns INVALID_PHYSICS_BODY on failure.
        uint32_t CreateBody(const PhysicsBodyDescriptor& descriptor,
                            const Float3& position,
                            const Float3& rotationDegrees = {0.0f, 0.0f, 0.0f});

        /// Remove and destroy a previously created body.
        void DestroyBody(uint32_t bodyId);

        /// Query the current world-space position of a body.
        Float3 GetBodyPosition(uint32_t bodyId) const;

        /// Query the current world-space rotation of a body as a quaternion
        /// packed into Float4 (x, y, z, w).
        Float4 GetBodyRotation(uint32_t bodyId) const;

        /// Set the world-space position of a kinematic body.
        void SetBodyPosition(uint32_t bodyId, const Float3& position);

        /// @return true after Initialise() and before Shutdown().
        bool IsInitialised() const { return m_initialised; }

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
        float m_fixedTimestep = 1.0f / 60.0f;
        float m_accumulator = 0.0f;
        bool m_initialised = false;
    };

} // namespace Wayfinder::Physics
