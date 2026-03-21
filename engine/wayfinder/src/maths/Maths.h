#pragma once

#include "../core/Types.h"
#include "wayfinder_exports.h"

namespace Wayfinder::Math3D
{
    WAYFINDER_API Matrix4 Identity();
    WAYFINDER_API Matrix4 Multiply(const Matrix4& lhs, const Matrix4& rhs);
    WAYFINDER_API Matrix4 ComposeTransform(const Float3& position, const Float3& rotationDegrees, const Float3& scale);
    WAYFINDER_API Float3 TransformPoint(const Matrix4& matrix, const Float3& point);
    WAYFINDER_API Float3 TransformDirection(const Matrix4& matrix, const Float3& direction);
    WAYFINDER_API Float3 Normalize(const Float3& value);
    WAYFINDER_API Float3 Add(const Float3& lhs, const Float3& rhs);
    WAYFINDER_API Float3 Scale(const Float3& value, float factor);
    WAYFINDER_API Float3 ExtractScale(const Matrix4& matrix);
}