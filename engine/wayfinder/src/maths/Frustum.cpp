#include "Frustum.h"

#include "Maths.h"

#include <algorithm>
#include <cmath>

namespace Wayfinder
{
    Frustum Frustum::ExtractPlanes(const Matrix4& viewProjection)
    {
        /// Gribb/Hartmann method: extract frustum planes from the rows of the
        /// combined view-projection matrix.  Each plane is the sum/difference
        /// of the 4th row and the i-th row.  The resulting planes point inward.
        ///
        /// For RH_ZO (right-handed, zero-to-one depth), the near and far planes
        /// differ from the classic [-1,1] derivation:
        ///   Near  = row2            (z >= 0)
        ///   Far   = row3 - row2     (z <= 1)

        Frustum frustum{};

        // GLM stores column-major: vp[col][row].
        // Row i of the matrix = (vp[0][i], vp[1][i], vp[2][i], vp[3][i]).

        auto row = [&](int i) -> Float4
        {
            return {viewProjection[0][i], viewProjection[1][i], viewProjection[2][i], viewProjection[3][i]};
        };

        const Float4 r0 = row(0);
        const Float4 r1 = row(1);
        const Float4 r2 = row(2);
        const Float4 r3 = row(3);

        // Left:   row3 + row0
        // Right:  row3 - row0
        // Bottom: row3 + row1
        // Top:    row3 - row1
        // Near:   row2            (RH_ZO: z >= 0)
        // Far:    row3 - row2     (RH_ZO: z <= 1)

        const std::array<Float4, 6> raw = {{
            r3 + r0, // Left
            r3 - r0, // Right
            r3 + r1, // Bottom
            r3 - r1, // Top
            r2,      // Near  (RH_ZO)
            r3 - r2, // Far   (RH_ZO)
        }};

        for (int i = 0; i < 6; ++i)
        {
            const Float3 normal{raw[i].x, raw[i].y, raw[i].z};
            const float length = std::sqrt(Maths::Dot(normal, normal));
            if (length > 0.0f)
            {
                const float inverseLength = 1.0f / length;
                frustum.Planes[i].Normal = normal * inverseLength;
                frustum.Planes[i].Distance = raw[i].w * inverseLength;
            }
        }

        return frustum;
    }

    bool Frustum::TestAABB(const AxisAlignedBounds& worldBounds) const
    {
        /// For each frustum plane, find the AABB's "positive vertex" (p-vertex) — the corner
        /// furthest along the plane normal.  If the p-vertex is behind the plane (negative
        /// signed distance), the entire AABB is outside the frustum.
        ///
        /// This is the standard "p-vertex / n-vertex" test and is conservative: it produces
        /// no false negatives but can produce false positives for boxes that straddle a corner.

        const Float3& min = worldBounds.Min;
        const Float3& max = worldBounds.Max;

        return std::ranges::all_of(Planes, [&](const FrustumPlane& plane)
        {
            const Float3 vertex{
                plane.Normal.x >= 0.0f ? max.x : min.x,
                plane.Normal.y >= 0.0f ? max.y : min.y,
                plane.Normal.z >= 0.0f ? max.z : min.z,
            };

            const float distance = Maths::Dot(plane.Normal, vertex) + plane.Distance;
            return distance >= 0.0f; // Not entirely outside this plane.
        });
    }

    bool Frustum::TestSphere(const BoundingSphere& sphere) const
    {
        return std::ranges::all_of(Planes, [&](const FrustumPlane& plane)
        {
            const float distance = Maths::Dot(plane.Normal, sphere.Centre) + plane.Distance;
            return distance >= -sphere.Radius; // Not entirely outside this plane.
        });
    }

    bool Frustum::TestBounds(const BoundingSphere& sphere, const AxisAlignedBounds& aabb) const
    {
        if (!TestSphere(sphere))
        {
            return false;
        }

        return TestAABB(aabb);
    }

} // namespace Wayfinder
