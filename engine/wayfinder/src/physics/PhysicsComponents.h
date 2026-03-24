#pragma once

#include "core/Types.h"
#include "wayfinder_exports.h"

#include <cstdint>
#include <format>
#include <functional>

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
     * @brief Typed handle for a Jolt physics body.
     *
     * Zero-cost wrapper over a raw @c uint32_t that prevents accidentally
     * passing an unrelated integer where a body handle is expected.
     * A default-constructed handle is invalid (sentinel = @c 0xFFFFFFFF,
     * matching Jolt's @c BodyID::cInvalidBodyID).
     */
    struct WAYFINDER_API PhysicsBodyId
    {
        /// Sentinel matching Jolt's invalid BodyID.
        static constexpr uint32_t INVALID_VALUE = 0xFFFFFFFF;

        /// Raw Jolt BodyID value. Public for interop with Jolt internals.
        uint32_t Value = INVALID_VALUE;

        constexpr PhysicsBodyId() = default;
        constexpr explicit PhysicsBodyId(uint32_t value) : Value(value) {}

        /// @return true if this handle refers to a valid Jolt body.
        [[nodiscard]] constexpr bool IsValid() const { return Value != INVALID_VALUE; }
        constexpr explicit operator bool() const { return IsValid(); }

        constexpr auto operator==(const PhysicsBodyId&) const -> bool = default;
        constexpr auto operator<=>(const PhysicsBodyId&) const = default;
    };

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

template<>
struct std::hash<Wayfinder::Physics::PhysicsBodyId>
{
    auto operator()(const Wayfinder::Physics::PhysicsBodyId& id) const noexcept -> size_t
    {
        return std::hash<uint32_t>{}(id.Value);
    }
};

template<>
struct std::formatter<Wayfinder::Physics::PhysicsBodyId> : std::formatter<uint32_t>
{
    auto format(const Wayfinder::Physics::PhysicsBodyId& id, auto& ctx) const
    {
        return std::formatter<uint32_t>::format(id.Value, ctx);
    }
};
