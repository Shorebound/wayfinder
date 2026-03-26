#include "Bounds.h"

#include <algorithm>
#include <cmath>

namespace Wayfinder
{
    AxisAlignedBounds TransformBounds(const AxisAlignedBounds& local, const Matrix4& transform)
    {
        /// Arvo's method (Graphics Gems, 1990): compute transformed AABB directly from
        /// the matrix columns.  For each output axis i, the new min/max is the translation
        /// component plus the sum of min/max contributions from each input axis j.
        /// This is ~3x faster than the 8-corner brute-force approach (18 mul+add vs 56).

        Float3 newMin{transform[3][0], transform[3][1], transform[3][2]};
        Float3 newMax = newMin;

        for (int j = 0; j < 3; ++j)
        {
            for (int i = 0; i < 3; ++i)
            {
                const float e = transform[j][i] * local.Min[j];
                const float f = transform[j][i] * local.Max[j];
                newMin[i] += std::min(e, f);
                newMax[i] += std::max(e, f);
            }
        }

        return AxisAlignedBounds{.Min = newMin, .Max = newMax};
    }

    BoundingSphere ComputeBoundingSphere(const AxisAlignedBounds& bounds)
    {
        const Float3 centre = (bounds.Min + bounds.Max) * 0.5f;
        const float radius = glm::length(bounds.Max - centre);
        return BoundingSphere{.Centre = centre, .Radius = radius};
    }

    BoundingSphere TransformSphere(const BoundingSphere& local, const Matrix4& transform)
    {
        /// Transform centre as a point; scale radius by the maximum column length
        /// (maximum axis scale factor) of the upper-3x3. This is conservative for
        /// non-uniform scale — the sphere may be slightly larger than necessary.
        const Float3 centre{transform * Float4(local.Centre, 1.0f)};

        const float scaleX = glm::length(Float3{transform[0]});
        const float scaleY = glm::length(Float3{transform[1]});
        const float scaleZ = glm::length(Float3{transform[2]});
        const float maxScale = std::max({scaleX, scaleY, scaleZ});

        return BoundingSphere{.Centre = centre, .Radius = local.Radius * maxScale};
    }

} // namespace Wayfinder
