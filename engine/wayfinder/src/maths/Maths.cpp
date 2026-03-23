#include "Maths.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Wayfinder::Maths
{
    Matrix4 Identity()
    {
        return {1.0f};
    }

    Matrix4 Multiply(const Matrix4& lhs, const Matrix4& rhs)
    {
        return lhs * rhs;
    }

    Matrix4 ComposeTransform(const Transform& transform)
    {
        const Radians rotationX = ToRadians(glm::dot(transform.RotationDegrees, Float3(1.0f, 0.0f, 0.0f)));
        const Radians rotationY = ToRadians(glm::dot(transform.RotationDegrees, Float3(0.0f, 1.0f, 0.0f)));
        const Radians rotationZ = ToRadians(glm::dot(transform.RotationDegrees, Float3(0.0f, 0.0f, 1.0f)));

        Matrix4 result(1.0f);
        result = glm::translate(result, transform.Position);
        result = glm::rotate(result, rotationZ, Float3(0.0f, 0.0f, 1.0f));
        result = glm::rotate(result, rotationY, Float3(0.0f, 1.0f, 0.0f));
        result = glm::rotate(result, rotationX, Float3(1.0f, 0.0f, 0.0f));
        result = glm::scale(result, transform.Scale);
        return result;
    }

    Float3 TransformPoint(const Matrix4& matrix, const Float3& point)
    {
        return {matrix * Float4(point, 1.0f)};
    }

    Float3 TransformDirection(const Matrix4& matrix, const Float3& direction)
    {
        return {matrix * Float4(direction, 0.0f)};
    }

    Float3 ExtractScale(const Matrix4& matrix)
    {
        return {
        Length(TransformDirection(matrix, {1.0f, 0.0f, 0.0f})),
        Length(TransformDirection(matrix, {0.0f, 1.0f, 0.0f})),
        Length(TransformDirection(matrix, {0.0f, 0.0f, 1.0f})),
        };
    }

    // ── Matrix builders ──────────────────────────────────────

    Matrix4 Translate(const Matrix4& matrix, const Float3& translation)
    {
        return glm::translate(matrix, translation);
    }

    Matrix4 Rotate(const Matrix4& matrix, Radians angle, const Float3& axis)
    {
        return glm::rotate(matrix, angle, axis);
    }

    Matrix4 ScaleMatrix(const Matrix4& matrix, const Float3& scale)
    {
        return glm::scale(matrix, scale);
    }

    // ── Camera / projection ──────────────────────────────────

    Matrix4 LookAt(const Float3& eye, const Float3& target, const Float3& up)
    {
        return glm::lookAt(eye, target, up);
    }

    Matrix4 PerspectiveRH_ZO(Radians fovY, float aspect, float zNear, float zFar)
    {
        return glm::perspectiveRH_ZO(fovY, aspect, zNear, zFar);
    }

    Matrix4 OrthoRH_ZO(float left, float right, float bottom, float top, float zNear, float zFar)
    {
        return glm::orthoRH_ZO(left, right, bottom, top, zNear, zFar);
    }

    // ── Vector operations ────────────────────────────────────

    Float3 Normalize(const Float3& value)
    {
        return glm::normalize(value);
    }

    Float3 Add(const Float3& lhs, const Float3& rhs)
    {
        return lhs + rhs;
    }

    Float3 Scale(const Float3& value, float factor)
    {
        return value * factor;
    }

    float Length(const Float3& value)
    {
        return glm::length(value);
    }

    Float3 Abs(const Float3& value)
    {
        return glm::abs(value);
    }

    float Max(float a, float b)
    {
        return glm::max(a, b);
    }

    Float3 Max(const Float3& a, const Float3& b)
    {
        return glm::max(a, b);
    }

    Matrix3 Transpose(const Matrix3& matrix)
    {
        return glm::transpose(matrix);
    }

    // ── Scalar interpolation / clamping ──────────────────────

    float Clamp(float value, float min, float max)
    {
        return glm::clamp(value, min, max);
    }

    float Mix(float a, float b, float t)
    {
        return glm::mix(a, b, t);
    }

    Float3 Mix(const Float3& a, const Float3& b, float t)
    {
        return glm::mix(a, b, t);
    }
} // namespace Wayfinder::Maths