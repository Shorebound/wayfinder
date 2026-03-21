#include "Maths.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Wayfinder::Math3D
{
    Matrix4 Identity()
    {
        return Matrix4(1.0f);
    }

    Matrix4 Multiply(const Matrix4& lhs, const Matrix4& rhs)
    {
        return lhs * rhs;
    }

    Matrix4 ComposeTransform(const Float3& position, const Float3& rotationDegrees, const Float3& scale)
    {
        Matrix4 result(1.0f);
        result = glm::translate(result, position);
        result = glm::rotate(result, ToRadians(rotationDegrees.z), Float3(0.0f, 0.0f, 1.0f));
        result = glm::rotate(result, ToRadians(rotationDegrees.y), Float3(0.0f, 1.0f, 0.0f));
        result = glm::rotate(result, ToRadians(rotationDegrees.x), Float3(1.0f, 0.0f, 0.0f));
        result = glm::scale(result, scale);
        return result;
    }

    Float3 TransformPoint(const Matrix4& matrix, const Float3& point)
    {
        return Float3(matrix * Float4(point, 1.0f));
    }

    Float3 TransformDirection(const Matrix4& matrix, const Float3& direction)
    {
        return Float3(matrix * Float4(direction, 0.0f));
    }

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

    Float3 ExtractScale(const Matrix4& matrix)
    {
        return Float3(
            glm::length(Float3(matrix[0])),
            glm::length(Float3(matrix[1])),
            glm::length(Float3(matrix[2])));
    }
} // namespace Wayfinder::Math3D