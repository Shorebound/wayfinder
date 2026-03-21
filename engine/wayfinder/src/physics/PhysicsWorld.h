#pragma once

#include "PhysicsComponents.h"
#include "../rendering/RenderTypes.h"
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

namespace Wayfinder
{
    /**
     * @brief Thin wrapper around Jolt's PhysicsSystem.
     *
     * Owns the Jolt world, temp allocator, job system and layer callbacks.
     * The engine creates exactly one instance via PhysicsSubsystem.
     */
    class WAYFINDER_API PhysicsWorld
    {
    public:
        PhysicsWorld();
        ~PhysicsWorld();

        // Non-copyable, non-movable (owns Jolt runtime state).
        PhysicsWorld(const PhysicsWorld&) = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;

        /// Initialise Jolt runtime, allocate the physics system.
        void Initialise();

        /// Tear down all bodies and free Jolt state.
        void Shutdown();

        /// Step the simulation by @p deltaTime seconds.
        void Step(float deltaTime);

        /// Create a Jolt body from component data and return its raw BodyID value.
        /// Returns INVALID_PHYSICS_BODY on failure.
        uint32_t CreateBody(const RigidBodyComponent& body,
                            const ColliderComponent& collider,
                            const Float3& position);

        /// Remove and destroy a previously created body.
        void DestroyBody(uint32_t bodyId);

        /// Query the current world-space position of a body.
        Float3 GetBodyPosition(uint32_t bodyId) const;

        /// Set the world-space position of a kinematic body.
        void SetBodyPosition(uint32_t bodyId, const Float3& position);

        /// @return true after Initialise() and before Shutdown().
        bool IsInitialised() const { return m_initialised; }

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
        bool m_initialised = false;
    };

} // namespace Wayfinder
