#pragma once

#include "core/Handle.h"
#include "core/Types.h"

#include <cstdint>

namespace Wayfinder::Physics
{
    /** @brief Motion type for a physics body. */
    enum class BodyType : uint8_t
    {
        Static,
        Dynamic,
        Kinematic
    };

    /** @brief Primitive collision shape type. */
    enum class ColliderShape : uint8_t
    {
        Box,
        Sphere,
        Capsule
    };

    /**
     * @brief Tag for the Jolt physics body opaque handle.
     *
     * Stores Jolt's raw BodyID value (index + sequence packed into a uint32_t).
     * Sentinel matches Jolt's @c BodyID::cInvalidBodyID.
     */
    struct PhysicsBodyTag
    {
        using ValueType = uint32_t;
        static constexpr ValueType INVALID = 0xFFFFFFFF;
    };

    /// Typed handle for a Jolt physics body. Prevents accidentally passing
    /// an unrelated integer where a body handle is expected.
    using PhysicsBodyId = OpaqueHandle<PhysicsBodyTag>;

    /// Sentinel constant for an invalid physics body handle.
    inline constexpr PhysicsBodyId INVALID_PHYSICS_BODY{};

    /**
     * @brief Describes the motion behaviour of a physics body.
     *
     * Authored fields (Type, Mass, GravityFactor, LinearDamping, AngularDamping,
     * LinearVelocity, AngularVelocity) are serialised to JSON.
     * RuntimeBodyId is set at runtime when the Jolt body is created and is
     * never serialised.
     */
    struct RigidBodyComponent
    {
        BodyType Type = BodyType::Dynamic;
        float Mass = 1.0f;
        float GravityFactor = 1.0f;
        float LinearDamping = 0.05f;
        float AngularDamping = 0.05f;
        Float3 LinearVelocity = {0.0f, 0.0f, 0.0f};
        Float3 AngularVelocity = {0.0f, 0.0f, 0.0f};

        /// Opaque Jolt BodyID — set at runtime, never serialised.
        /// Deliberately preserved across copies: Flecs copies components
        /// between tables on archetype changes, and resetting the handle
        /// here would silently orphan the Jolt body.  Duplicate-creation
        /// is prevented by the PhysicsCreateBodies observer guard, and
        /// cleanup is handled by PhysicsDestroyBodies on remove/destruct.
        PhysicsBodyId RuntimeBodyId;
    };

    /**
     * @brief Describes the collision shape attached to a physics body.
     *
     * Only the fields relevant to the chosen Shape are used:
     * - Box     → HalfExtents
     * - Sphere  → Radius
     * - Capsule → Radius, Height
     */
    struct ColliderComponent
    {
        ColliderShape Shape = ColliderShape::Box;
        Float3 HalfExtents = {0.5f, 0.5f, 0.5f};
        float Radius = 0.5f;
        float Height = 1.0f;
        float Friction = 0.2f;
        float Restitution = 0.0f;
    };

} // namespace Wayfinder::Physics
