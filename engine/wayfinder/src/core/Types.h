#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Wayfinder
{

    // ── Engine Math Aliases ──────────────────────────────────

    using Float2 = glm::vec2;
    using Float3 = glm::vec3;
    using Float4 = glm::vec4;

    using Int2 = glm::ivec2;
    using Int3 = glm::ivec3;
    using Int4 = glm::ivec4;

    using UInt2 = glm::uvec2;
    using UInt3 = glm::uvec3;
    using UInt4 = glm::uvec4;

    using Double2 = glm::dvec2;
    using Double3 = glm::dvec3;
    using Double4 = glm::dvec4;

    using Bool2 = glm::bvec2;
    using Bool3 = glm::bvec3;
    using Bool4 = glm::bvec4;

    using Matrix2 = glm::mat2;
    using Matrix2x3 = glm::mat2x3;
    using Matrix2x4 = glm::mat2x4;

    using Matrix3x2 = glm::mat3x2;
    using Matrix3 = glm::mat3;
    using Matrix3x4 = glm::mat3x4;

    using Matrix4x2 = glm::mat4x2;
    using Matrix4x3 = glm::mat4x3;
    using Matrix4 = glm::mat4;

    using Quaternion = glm::quat;
    using Translation = Float3;
    using Rotation = Float3;
    using Scale = Float3;

    using Degrees = float;
    using Radians = float;

    // ── Coordinate-system / convention enums ─────────────────

    enum class Axis
    {
        X,
        Y,
        Z
    };

    enum class RotationOrder
    {
        XYZ,
        XZY,
        YXZ,
        YZX,
        ZXY,
        ZYX
    };

    enum class Handedness
    {
        Left,
        Right
    };

    // ── Colour ────────────────────────────────────────────────

    /** @brief 8-bit sRGB colour (authored / on-disk representation). */
    struct Colour
    {
        uint8_t r, g, b, a;

        static Colour White()
        {
            return {.r = 255, .g = 255, .b = 255, .a = 255};
        }
        static Colour Black()
        {
            return {.r = 0, .g = 0, .b = 0, .a = 255};
        }
        static Colour Red()
        {
            return {.r = 255, .g = 0, .b = 0, .a = 255};
        }
        static Colour Green()
        {
            return {.r = 0, .g = 255, .b = 0, .a = 255};
        }
        static Colour Blue()
        {
            return {.r = 0, .g = 0, .b = 255, .a = 255};
        }
        static Colour Yellow()
        {
            return {.r = 255, .g = 255, .b = 0, .a = 255};
        }
        static Colour Gray()
        {
            return {.r = 128, .g = 128, .b = 128, .a = 255};
        }
        static Colour DarkGray()
        {
            return {.r = 80, .g = 80, .b = 80, .a = 255};
        }
    };

    // ── LinearColour ─────────────────────────────────────────

    /** @brief Float colour in linear space for GPU-side work.
     *
     * Conversions from authored sRGB colour happen once at load/extract time.
     */
    struct LinearColour
    {
        float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;

        static LinearColour White()
        {
            return {1.0f, 1.0f, 1.0f, 1.0f};
        }
        static LinearColour Black()
        {
            return {0.0f, 0.0f, 0.0f, 1.0f};
        }

        static LinearColour FromColour(const Colour& c)
        {
            return {
            .r = static_cast<float>(c.r) / 255.0f,
            .g = static_cast<float>(c.g) / 255.0f,
            .b = static_cast<float>(c.b) / 255.0f,
            .a = static_cast<float>(c.a) / 255.0f,
            };
        }

        Float4 ToFloat4() const
        {
            return {r, g, b, a};
        }
        Float3 ToFloat3() const
        {
            return {r, g, b};
        }
    };

} // namespace Wayfinder
