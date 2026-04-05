#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

namespace Wayfinder
{

    using Float = float;
    using Double = double;

    using Byte = uint8_t;
    using Int32 = int32_t;
    using UInt32 = uint32_t;
    using Int64 = int64_t;
    using UInt64 = uint64_t;

    using Timestep = double;
    using FrameIndex = UInt64;

    using Bool = bool;
    using String = std::string;
    using StringView = std::string_view;
    using Text = std::string;

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

    inline constexpr Float3 Up{0.f, 1.f, 0.f};
    inline constexpr Float3 Down{0.f, -1.f, 0.f};
    inline constexpr Float3 Right{1.f, 0.f, 0.f};
    inline constexpr Float3 Left{-1.f, 0.f, 0.f};
    inline constexpr Float3 Forward{0.f, 0.f, -1.f};
    inline constexpr Float3 Back{0.f, 0.f, 1.f};
    inline constexpr Float3 One{1.f, 1.f, 1.f};
    inline constexpr Float3 Zero{0.f, 0.f, 0.f};

    /// Unity-style (Y-up, left-handed)
    namespace Unity
    {
        inline constexpr Float3 Up{0.f, 1.f, 0.f};
        inline constexpr Float3 Down{0.f, -1.f, 0.f};
        inline constexpr Float3 Right{1.f, 0.f, 0.f};
        inline constexpr Float3 Left{-1.f, 0.f, 0.f};
        inline constexpr Float3 Forward{0.f, 0.f, 1.f};
        inline constexpr Float3 Back{0.f, 0.f, -1.f};
    }

    /// Unreal-style (Z-up, left-handed)
    namespace Unreal
    {
        inline constexpr Float3 Up{0.f, 0.f, 1.f};
        inline constexpr Float3 Down{0.f, 0.f, -1.f};
        inline constexpr Float3 Right{0.f, 1.f, 0.f};
        inline constexpr Float3 Left{0.f, -1.f, 0.f};
        inline constexpr Float3 Forward{1.f, 0.f, 0.f};
        inline constexpr Float3 Back{-1.f, 0.f, 0.f};
    }

    /// Godot / OpenGL (Y-up, right-handed)
    namespace Godot
    {
        inline constexpr Float3 Up{0.f, 1.f, 0.f};
        inline constexpr Float3 Down{0.f, -1.f, 0.f};
        inline constexpr Float3 Right{1.f, 0.f, 0.f};
        inline constexpr Float3 Left{-1.f, 0.f, 0.f};
        inline constexpr Float3 Forward{0.f, 0.f, -1.f};
        inline constexpr Float3 Back{0.f, 0.f, 1.f};
    }

    /// Source / Blender (Z-up, right-handed)
    namespace Source
    {
        inline constexpr Float3 Up{0.f, 0.f, 1.f};
        inline constexpr Float3 Down{0.f, 0.f, -1.f};
        inline constexpr Float3 Right{1.f, 0.f, 0.f};
        inline constexpr Float3 Left{-1.f, 0.f, 0.f};
        inline constexpr Float3 Forward{0.f, 1.f, 0.f};
        inline constexpr Float3 Back{0.f, -1.f, 0.f};
    }

    struct Transform
    {
        Float3 Position = {0.0f, 0.0f, 0.0f};
        Float3 RotationDegrees = {0.0f, 0.0f, 0.0f};
        Float3 Scale = {1.0f, 1.0f, 1.0f};
    };

    //  Coordinate-system / convention enums

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

    //  Colour

    /** @brief 8-bit sRGB colour (authored / on-disk representation). */
    struct Colour
    {
        Byte r = 255, g = 255, b = 255, a = 255;

        static constexpr Colour White()
        {
            return {.r = 255, .g = 255, .b = 255, .a = 255};
        }
        static constexpr Colour Black()
        {
            return {.r = 0, .g = 0, .b = 0, .a = 255};
        }
        static constexpr Colour Red()
        {
            return {.r = 255, .g = 0, .b = 0, .a = 255};
        }
        static constexpr Colour Green()
        {
            return {.r = 0, .g = 255, .b = 0, .a = 255};
        }
        static constexpr Colour Blue()
        {
            return {.r = 0, .g = 0, .b = 255, .a = 255};
        }
        static constexpr Colour Yellow()
        {
            return {.r = 255, .g = 255, .b = 0, .a = 255};
        }
        static constexpr Colour Gray()
        {
            return {.r = 128, .g = 128, .b = 128, .a = 255};
        }
        static constexpr Colour DarkGray()
        {
            return {.r = 80, .g = 80, .b = 80, .a = 255};
        }
    };

    //  LinearColour

    /**
     * @brief Float colour in linear space for GPU-side work.
     *
     * Backed by a Float4 for direct GPU/vector-math interop.
     * Conversions from authored sRGB colour happen once at load/extract time.
     */
    struct LinearColour
    {
        Float4 Data{1.0f, 1.0f, 1.0f, 1.0f};

        constexpr LinearColour() = default;
        constexpr LinearColour(float r, float g, float b, float a) : Data(r, g, b, a) {}
        constexpr explicit LinearColour(const Float4& v) : Data(v) {}

        static constexpr LinearColour White()
        {
            return {1.0f, 1.0f, 1.0f, 1.0f};
        }
        static constexpr LinearColour Black()
        {
            return {0.0f, 0.0f, 0.0f, 1.0f};
        }

        static LinearColour FromColour(const Colour& c)
        {
            return {
                static_cast<float>(c.r) / 255.0f,
                static_cast<float>(c.g) / 255.0f,
                static_cast<float>(c.b) / 255.0f,
                static_cast<float>(c.a) / 255.0f,
            };
        }

        /// Implicit conversion to Float4 for GPU uploads and vector math.
        operator const Float4&() const
        {
            return Data;
        }
        operator Float4&()
        {
            return Data;
        }
        Float3 ToFloat3() const
        {
            return {Data.r, Data.g, Data.b};
        }
    };

} // namespace Wayfinder
