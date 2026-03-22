#pragma once

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

    /// Sentinel value matching Jolt's invalid BodyID.
    inline constexpr uint32_t INVALID_PHYSICS_BODY = 0xFFFFFFFF;

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
        uint32_t RuntimeBodyId = INVALID_PHYSICS_BODY;
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

        ColliderComponent() = default;
        ColliderComponent(const ColliderComponent&) = default;
        ColliderComponent& operator=(const ColliderComponent&) = default;
    };

} // namespace Wayfinder::Physics
