#pragma once

#include "../rendering/RenderTypes.h"

#include <cstdint>

namespace Wayfinder
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
    static constexpr uint32_t INVALID_PHYSICS_BODY = 0xFFFFFFFF;

    /**
     * @brief Describes the motion behaviour of a physics body.
     *
     * Authored fields (Type, Mass, GravityFactor, LinearDamping, AngularDamping,
     * LinearVelocity, AngularVelocity) are serialised to TOML.
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
        uint32_t RuntimeBodyId = INVALID_PHYSICS_BODY;

        RigidBodyComponent() = default;
        RigidBodyComponent(const RigidBodyComponent&) = default;
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
    };

} // namespace Wayfinder
