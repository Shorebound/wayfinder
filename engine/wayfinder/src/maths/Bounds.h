#pragma once

#include "core/Types.h"
#include "wayfinder_exports.h"

#include <array>

namespace Wayfinder
{
    struct AxisAlignedBounds
    {
        Float3 Min{0.0f};
        Float3 Max{0.0f};
    };

    struct BoundingSphere
    {
        Float3 Centre{0.0f};
        float Radius = 0.0f;
    };

    /**
     * @brief Transform a local-space AABB by an affine matrix, returning the world-space enclosing AABB.
     *
     * Uses Arvo's optimised method (Graphics Gems, 1990): computes the transformed AABB directly
     * from the matrix columns without expanding to 8 corners. ~3x faster than the brute-force approach.
     *
     * @note Only correct for affine transforms (no projective division). Model/world matrices are always affine.
     */
    WAYFINDER_API AxisAlignedBounds TransformBounds(const AxisAlignedBounds& local, const Matrix4& transform);

    /**
     * @brief Compute a bounding sphere from an AABB (centre + half-diagonal radius).
     */
    WAYFINDER_API BoundingSphere ComputeBoundingSphere(const AxisAlignedBounds& bounds);

    /**
     * @brief Transform a bounding sphere by an affine matrix.
     *
     * Transforms the centre and scales the radius by the maximum axis scale factor.
     */
    WAYFINDER_API BoundingSphere TransformSphere(const BoundingSphere& local, const Matrix4& transform);

} // namespace Wayfinder
