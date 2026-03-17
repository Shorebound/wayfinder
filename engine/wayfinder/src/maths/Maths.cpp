#include "Maths.h"

#include "raylib.h"
#include "raymath.h"

namespace
{
    ::Vector3 ToRaylib(const Wayfinder::Float3& value)
    {
        return {value.x, value.y, value.z};
    }

    Wayfinder::Float3 ToFloat3(const ::Vector3& value)
    {
        return {value.x, value.y, value.z};
    }

    ::Matrix ToRaylib(const Wayfinder::Matrix4& matrix)
    {
        return {
            matrix.m0,
            matrix.m4,
            matrix.m8,
            matrix.m12,
            matrix.m1,
            matrix.m5,
            matrix.m9,
            matrix.m13,
            matrix.m2,
            matrix.m6,
            matrix.m10,
            matrix.m14,
            matrix.m3,
            matrix.m7,
            matrix.m11,
            matrix.m15,
        };
    }

    Wayfinder::Matrix4 ToMatrix4(const ::Matrix& matrix)
    {
        Wayfinder::Matrix4 result;
        result.m0 = matrix.m0;
        result.m4 = matrix.m4;
        result.m8 = matrix.m8;
        result.m12 = matrix.m12;
        result.m1 = matrix.m1;
        result.m5 = matrix.m5;
        result.m9 = matrix.m9;
        result.m13 = matrix.m13;
        result.m2 = matrix.m2;
        result.m6 = matrix.m6;
        result.m10 = matrix.m10;
        result.m14 = matrix.m14;
        result.m3 = matrix.m3;
        result.m7 = matrix.m7;
        result.m11 = matrix.m11;
        result.m15 = matrix.m15;
        return result;
    }
}

namespace Wayfinder::Math3D
{
    Matrix4 Identity()
    {
        return Matrix4::Identity();
    }

    Matrix4 Multiply(const Matrix4& lhs, const Matrix4& rhs)
    {
        return ToMatrix4(::MatrixMultiply(ToRaylib(lhs), ToRaylib(rhs)));
    }

    Matrix4 ComposeTransform(const Float3& position, const Float3& rotationDegrees, const Float3& scale)
    {
        const ::Matrix matScale = ::MatrixScale(scale.x, scale.y, scale.z);
        const ::Matrix matRotation = ::MatrixRotateZYX({
            rotationDegrees.x * DEG2RAD,
            rotationDegrees.y * DEG2RAD,
            rotationDegrees.z * DEG2RAD,
        });
        const ::Matrix matTranslation = ::MatrixTranslate(position.x, position.y, position.z);
        return ToMatrix4(::MatrixMultiply(::MatrixMultiply(matScale, matRotation), matTranslation));
    }

    Float3 TransformPoint(const Matrix4& matrix, const Float3& point)
    {
        return ToFloat3(::Vector3Transform(ToRaylib(point), ToRaylib(matrix)));
    }

    Float3 TransformDirection(const Matrix4& matrix, const Float3& direction)
    {
        const Float3 origin = TransformPoint(matrix, {0.0f, 0.0f, 0.0f});
        const Float3 transformed = TransformPoint(matrix, direction);
        return {transformed.x - origin.x, transformed.y - origin.y, transformed.z - origin.z};
    }

    Float3 Normalize(const Float3& value)
    {
        return ToFloat3(::Vector3Normalize(ToRaylib(value)));
    }

    Float3 Add(const Float3& lhs, const Float3& rhs)
    {
        return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
    }

    Float3 Scale(const Float3& value, float factor)
    {
        return {value.x * factor, value.y * factor, value.z * factor};
    }

    Float3 ExtractScale(const Matrix4& matrix)
    {
        const Float3 origin = TransformPoint(matrix, {0.0f, 0.0f, 0.0f});
        const Float3 xAxis = TransformPoint(matrix, {1.0f, 0.0f, 0.0f});
        const Float3 yAxis = TransformPoint(matrix, {0.0f, 1.0f, 0.0f});
        const Float3 zAxis = TransformPoint(matrix, {0.0f, 0.0f, 1.0f});

        return {
            ::Vector3Distance(ToRaylib(origin), ToRaylib(xAxis)),
            ::Vector3Distance(ToRaylib(origin), ToRaylib(yAxis)),
            ::Vector3Distance(ToRaylib(origin), ToRaylib(zAxis)),
        };
    }
} // namespace Wayfinder::Math3D