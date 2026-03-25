#pragma once

#include "Bounds.h"
#include "core/Types.h"
#include "wayfinder_exports.h"

#include <array>

namespace Wayfinder
{
    /**
     * @brief A half-space plane in the form Ax + By + Cz + D = 0.
     *
     * The normal (A, B, C) points inward — a point with positive signed distance is inside the frustum.
     */
    struct FrustumPlane
    {
        Float3 Normal{0.0f};
        float Distance = 0.0f;
    };

    /**
     * @brief Six-plane view frustum extracted from a view-projection matrix.
     *
     * Planes point inward: a point on the positive side of all six planes is visible.
     * Use `ExtractFrustumPlanes()` to construct from a combined view-projection matrix,
     * then `TestAABB()` to cull world-space axis-aligned bounding boxes.
     */
    struct WAYFINDER_API Frustum
    {
        enum PlaneIndex : uint8_t
        {
            Left = 0,
            Right = 1,
            Bottom = 2,
            Top = 3,
            Near = 4,
            Far = 5,
        };

        std::array<FrustumPlane, 6> Planes{};

        /**
         * @brief Extract frustum planes from a combined view-projection matrix.
         *
         * Uses the Gribb/Hartmann method. Planes are normalised so that
         * `Dot(Normal, point) + Distance` gives the signed distance from the plane.
         *
         * Works with both perspective and orthographic projections.
         * Assumes right-handed, zero-to-one depth (RH_ZO) — matching `PerspectiveRH_ZO`.
         */
        static Frustum ExtractPlanes(const Matrix4& viewProjection);

        /**
         * @brief Test a world-space AABB against the frustum.
         *
         * @return true if the AABB is at least partially inside the frustum (i.e. should be rendered).
         *         Returns true for intersecting AABBs (conservative — no false negatives).
         */
        bool TestAABB(const AxisAlignedBounds& worldBounds) const;

        /**
         * @brief Test a world-space bounding sphere against the frustum.
         *
         * @return true if the sphere is at least partially inside the frustum.
         *         Cheaper than TestAABB (one dot product per plane).
         */
        bool TestSphere(const BoundingSphere& sphere) const;

        /**
         * @brief Two-tier visibility test: sphere pre-test, then AABB.
         *
         * Early-rejects with the cheaper sphere test. Objects that pass the sphere test
         * are refined with the tighter AABB test to reduce false positives.
         */
        bool TestBounds(const BoundingSphere& sphere, const AxisAlignedBounds& aabb) const;
    };

} // namespace Wayfinder
