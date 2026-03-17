#pragma once

#include <cmath>

#include "raylib.h"
#include "raymath.h"

namespace Wayfinder::Math3D
{
    inline Matrix ComposeTransform(const Vector3& position, const Vector3& rotationDegrees, const Vector3& scale)
    {
        const Matrix matScale = MatrixScale(scale.x, scale.y, scale.z);
        const Matrix matRotation = MatrixRotateZYX({
            rotationDegrees.x * DEG2RAD,
            rotationDegrees.y * DEG2RAD,
            rotationDegrees.z * DEG2RAD
        });
        const Matrix matTranslation = MatrixTranslate(position.x, position.y, position.z);
        return MatrixMultiply(MatrixMultiply(matScale, matRotation), matTranslation);
    }

    inline Vector3 TransformPoint(const Matrix& matrix, const Vector3& point)
    {
        return Vector3Transform(point, matrix);
    }

    inline Vector3 TransformDirection(const Matrix& matrix, const Vector3& direction)
    {
        const Vector3 origin = TransformPoint(matrix, {0.0f, 0.0f, 0.0f});
        const Vector3 transformed = TransformPoint(matrix, direction);
        return Vector3Subtract(transformed, origin);
    }

    inline Vector3 GetBoundingBoxCenter(const BoundingBox& box)
    {
        return Vector3Add(box.min, Vector3Scale(Vector3Subtract(box.max, box.min), 0.5f));
    }

    inline Vector3 GetBoundingBoxSize(const BoundingBox& box)
    {
        return Vector3Subtract(box.max, box.min);
    }

    inline Vector3 ExtractScale(const Matrix& matrix)
    {
        const Vector3 origin = TransformPoint(matrix, {0.0f, 0.0f, 0.0f});
        const Vector3 xAxis = TransformPoint(matrix, {1.0f, 0.0f, 0.0f});
        const Vector3 yAxis = TransformPoint(matrix, {0.0f, 1.0f, 0.0f});
        const Vector3 zAxis = TransformPoint(matrix, {0.0f, 0.0f, 1.0f});
        return {
            Vector3Length(Vector3Subtract(xAxis, origin)),
            Vector3Length(Vector3Subtract(yAxis, origin)),
            Vector3Length(Vector3Subtract(zAxis, origin))
        };
    }

    inline BoundingBox ScaleBoundingBox(const BoundingBox& box, const Vector3& scale)
    {
        const Vector3 center = GetBoundingBoxCenter(box);
        const Vector3 halfExtent = Vector3Multiply(Vector3Scale(GetBoundingBoxSize(box), 0.5f), {
            fabsf(scale.x),
            fabsf(scale.y),
            fabsf(scale.z)
        });
        return BoundingBox{
            Vector3Subtract(center, halfExtent),
            Vector3Add(center, halfExtent)
        };
    }

    inline RayCollision GetRayCollisionBoxOriented(const Ray& ray, const BoundingBox& box, const Matrix& transform)
    {
        RayCollision hit = { 0 };

        const BoundingBox scaledBox = ScaleBoundingBox(box, ExtractScale(transform));
        const Vector3 xAxis = Vector3Normalize(TransformDirection(transform, {1.0f, 0.0f, 0.0f}));
        const Vector3 yAxis = Vector3Normalize(TransformDirection(transform, {0.0f, 1.0f, 0.0f}));
        const Vector3 zAxis = Vector3Normalize(TransformDirection(transform, {0.0f, 0.0f, 1.0f}));

        float tMin = 0.0f;
        float tMax = 100000.0f;

        const Vector3 boxPosition = TransformPoint(transform, {0.0f, 0.0f, 0.0f});
        const Vector3 delta = Vector3Subtract(boxPosition, ray.position);

        const auto testAxis = [&](const Vector3& axis, float minValue, float maxValue) -> bool
        {
            const float e = Vector3DotProduct(axis, delta);
            const float f = Vector3DotProduct(ray.direction, axis);

            if (fabsf(f) > 0.001f)
            {
                float t1 = (e + minValue) / f;
                float t2 = (e + maxValue) / f;

                if (t1 > t2)
                {
                    const float swap = t1;
                    t1 = t2;
                    t2 = swap;
                }

                if (t2 < tMax)
                {
                    tMax = t2;
                }

                if (t1 > tMin)
                {
                    tMin = t1;
                }

                return tMax >= tMin;
            }

            return -e + minValue <= 0.0f && -e + maxValue >= 0.0f;
        };

        if (!testAxis(xAxis, scaledBox.min.x, scaledBox.max.x)
            || !testAxis(yAxis, scaledBox.min.y, scaledBox.max.y)
            || !testAxis(zAxis, scaledBox.min.z, scaledBox.max.z))
        {
            return hit;
        }

        hit.hit = true;
        hit.distance = tMin;
        hit.point = Vector3Add(ray.position, Vector3Scale(ray.direction, tMin));
        return hit;
    }
}