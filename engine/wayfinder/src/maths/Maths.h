#pragma once

#include "core/Types.h"
#include "wayfinder_exports.h"

namespace Wayfinder::Maths
{
    // ── Angle conversion ─────────────────────────────────────

    inline Radians ToRadians(Degrees degrees) { return glm::radians(degrees); }
    inline Degrees ToDegrees(Radians radians) { return glm::degrees(radians); }

    // ── Quaternion / matrix conversion ───────────────────────

    inline Quaternion EulerDegreesToQuaternion(const Rotation& eulerDegrees)
    {
        return Quaternion(glm::radians(eulerDegrees));
    }

    inline Quaternion ToQuaternion(const Matrix4& mat)
    {
        return glm::quat_cast(mat);
    }

    inline Matrix4 ToMatrix4(const Quaternion& q)
    {
        return glm::mat4_cast(q);
    }

    // ── Transform operations ─────────────────────────────────

    WAYFINDER_API Matrix4 Identity();
    WAYFINDER_API Matrix4 Multiply(const Matrix4& lhs, const Matrix4& rhs);
    WAYFINDER_API Matrix4 ComposeTransform(const Float3& position, const Float3& rotationDegrees, const Float3& scale);
    WAYFINDER_API Float3 TransformPoint(const Matrix4& matrix, const Float3& point);
    WAYFINDER_API Float3 TransformDirection(const Matrix4& matrix, const Float3& direction);
    WAYFINDER_API Float3 ExtractScale(const Matrix4& matrix);

    // ── Matrix builders ──────────────────────────────────────

    WAYFINDER_API Matrix4 Translate(const Matrix4& matrix, const Float3& translation);
    WAYFINDER_API Matrix4 Rotate(const Matrix4& matrix, Radians angle, const Float3& axis);
    WAYFINDER_API Matrix4 ScaleMatrix(const Matrix4& matrix, const Float3& scale);

    // ── Camera / projection ──────────────────────────────────

    WAYFINDER_API Matrix4 LookAt(const Float3& eye, const Float3& target, const Float3& up);
    WAYFINDER_API Matrix4 PerspectiveRH_ZO(Radians fovY, float aspect, float zNear, float zFar);
    WAYFINDER_API Matrix4 OrthoRH_ZO(float left, float right, float bottom, float top, float zNear, float zFar);

    // ── Vector operations ────────────────────────────────────

    WAYFINDER_API Float3 Normalize(const Float3& value);
    WAYFINDER_API Float3 Add(const Float3& lhs, const Float3& rhs);
    WAYFINDER_API Float3 Scale(const Float3& value, float factor);
    WAYFINDER_API float Length(const Float3& value);

    WAYFINDER_API Float3 Abs(const Float3& value);
    WAYFINDER_API float Max(float a, float b);
    WAYFINDER_API Float3 Max(const Float3& a, const Float3& b);
    WAYFINDER_API Matrix3 Transpose(const Matrix3& matrix);

    // ── Scalar interpolation / clamping ──────────────────────

    WAYFINDER_API float Clamp(float value, float min, float max);
    WAYFINDER_API float Mix(float a, float b, float t);
    WAYFINDER_API Float3 Mix(const Float3& a, const Float3& b, float t);
}