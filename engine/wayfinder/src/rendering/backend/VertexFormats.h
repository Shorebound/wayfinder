#pragma once

#include "core/Types.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Wayfinder
{
    // ── Vertex Structures ────────────────────────────────────

    struct VertexPosition
    {
        Float3 Position;
    };

    struct VertexPositionColour
    {
        Float3 Position;
        Float3 Colour;
    };

    struct VertexPositionNormalUV
    {
        Float3 Position;
        Float3 Normal;
        Float2 UV;
    };

    /// Tangent.xyz + handedness sign in w (MikkTSpace convention).
    struct VertexPositionNormalUVTangent
    {
        Float3 Position;
        Float3 Normal;
        Float2 UV;
        Float4 Tangent;
    };

    struct VertexPositionNormalColour
    {
        Float3 Position;
        Float3 Normal;
        Float3 Colour;
    };

    // ── Vertex Layout Descriptors (SDL_GPU-oriented) ─────────

    // These are lightweight structs that mirror SDL_GPUVertexAttribute
    // without pulling in SDL headers. The backend translates them.

    enum class VertexAttributeFormat : uint8_t
    {
        Float2,
        Float3,
        Float4,
    };

    struct VertexAttribute
    {
        uint32_t Location;
        uint32_t Offset;
        VertexAttributeFormat Format;
    };

    struct VertexLayout
    {
        uint32_t Stride = 0;
        const VertexAttribute* Attributes = nullptr;
        uint32_t AttributeCount = 0;
    };

    /**
     * @brief Compares two vertex layouts for equivalence.
     * @param a First layout to compare.
     * @param b Second layout to compare.
     * @return True when both layouts describe the same vertex format
     *         (same stride, count, and per-attribute location/offset/format).
     */
    inline bool VertexLayoutsMatch(const VertexLayout& a, const VertexLayout& b)
    {
        if (a.Stride != b.Stride || a.AttributeCount != b.AttributeCount)
        {
            return false;
        }
        if (a.AttributeCount > 0 && (a.Attributes == nullptr || b.Attributes == nullptr))
        {
            return false;
        }
        for (uint32_t i = 0; i < a.AttributeCount; ++i)
        {
            if (a.Attributes[i].Location != b.Attributes[i].Location || a.Attributes[i].Offset != b.Attributes[i].Offset || a.Attributes[i].Format != b.Attributes[i].Format)
            {
                return false;
            }
        }
        return true;
    }

    // ── Pre-built Layouts ────────────────────────────────────

    namespace VertexLayouts
    {
        // Empty layout for fullscreen passes using SV_VertexID (no vertex buffer)
        inline constexpr VertexLayout EMPTY =
        {
            .Stride = 0,
            .Attributes = nullptr,
            .AttributeCount = 0,
        };

        inline constexpr std::array<VertexAttribute, 1> POSITION_ATTRIBUTES = {{
            {0, offsetof(VertexPosition, Position), VertexAttributeFormat::Float3},
        }};

        inline constexpr VertexLayout POSITION =
        {
            .Stride = sizeof(VertexPosition),
            .Attributes = POSITION_ATTRIBUTES.data(),
            .AttributeCount = static_cast<uint32_t>(POSITION_ATTRIBUTES.size()),
        };

        inline constexpr std::array<VertexAttribute, 2> POSITION_COLOUR_ATTRIBUTES = {{
            {0, offsetof(VertexPositionColour, Position), VertexAttributeFormat::Float3},
            {1, offsetof(VertexPositionColour, Colour), VertexAttributeFormat::Float3},
        }};

        inline constexpr VertexLayout POSITION_COLOUR =
        {
            .Stride = sizeof(VertexPositionColour),
            .Attributes = POSITION_COLOUR_ATTRIBUTES.data(),
            .AttributeCount = static_cast<uint32_t>(POSITION_COLOUR_ATTRIBUTES.size()),
        };

        inline constexpr std::array<VertexAttribute, 3> POSITION_NORMAL_UV_ATTRIBUTES = {{
            {0, offsetof(VertexPositionNormalUV, Position), VertexAttributeFormat::Float3},
            {1, offsetof(VertexPositionNormalUV, Normal), VertexAttributeFormat::Float3},
            {2, offsetof(VertexPositionNormalUV, UV), VertexAttributeFormat::Float2},
        }};

        inline constexpr VertexLayout POSITION_NORMAL_UV =
        {
            .Stride = sizeof(VertexPositionNormalUV),
            .Attributes = POSITION_NORMAL_UV_ATTRIBUTES.data(),
            .AttributeCount = static_cast<uint32_t>(POSITION_NORMAL_UV_ATTRIBUTES.size()),
        };

        inline constexpr std::array<VertexAttribute, 4> POSITION_NORMAL_UV_TANGENT_ATTRIBUTES = {{
            {0, offsetof(VertexPositionNormalUVTangent, Position), VertexAttributeFormat::Float3},
            {1, offsetof(VertexPositionNormalUVTangent, Normal), VertexAttributeFormat::Float3},
            {2, offsetof(VertexPositionNormalUVTangent, UV), VertexAttributeFormat::Float2},
            {3, offsetof(VertexPositionNormalUVTangent, Tangent), VertexAttributeFormat::Float4},
        }};

        inline constexpr VertexLayout POSITION_NORMAL_UV_TANGENT =
        {
            .Stride = sizeof(VertexPositionNormalUVTangent),
            .Attributes = POSITION_NORMAL_UV_TANGENT_ATTRIBUTES.data(),
            .AttributeCount = static_cast<uint32_t>(POSITION_NORMAL_UV_TANGENT_ATTRIBUTES.size()),
        };

        inline constexpr std::array<VertexAttribute, 3> POSITION_NORMAL_COLOUR_ATTRIBUTES = {{
            {0, offsetof(VertexPositionNormalColour, Position), VertexAttributeFormat::Float3},
            {1, offsetof(VertexPositionNormalColour, Normal), VertexAttributeFormat::Float3},
            {2, offsetof(VertexPositionNormalColour, Colour), VertexAttributeFormat::Float3},
        }};

        inline constexpr VertexLayout POSITION_NORMAL_COLOUR =
        {
            .Stride = sizeof(VertexPositionNormalColour),
            .Attributes = POSITION_NORMAL_COLOUR_ATTRIBUTES.data(),
            .AttributeCount = static_cast<uint32_t>(POSITION_NORMAL_COLOUR_ATTRIBUTES.size()),
        };
    }

} // namespace Wayfinder
