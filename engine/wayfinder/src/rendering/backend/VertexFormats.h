#pragma once

#include "core/Types.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Wayfinder
{
    // ── Vertex Structures ────────────────────────────────────

    struct VertexPos
    {
        Float3 Position;
    };

    struct VertexPosColour
    {
        Float3 Position;
        Float3 Colour;
    };

    struct VertexPosNormalUV
    {
        Float3 Position;
        Float3 Normal;
        Float2 UV;
    };

    struct VertexPosNormalColour
    {
        Float3 Position;
        Float3 Normal;
        Float3 Colour;
    };

    // ── Vertex Layout Descriptors (SDL_GPU-oriented) ─────────

    // These are lightweight structs that mirror SDL_GPUVertexAttribute
    // without pulling in SDL headers. The backend translates them.

    enum class VertexAttribFormat : uint8_t
    {
        Float2,
        Float3,
        Float4,
    };

    struct VertexAttrib
    {
        uint32_t location;
        uint32_t offset;
        VertexAttribFormat format;
    };

    struct VertexLayout
    {
        uint32_t stride = 0;
        const VertexAttrib* attribs = nullptr;
        uint32_t attribCount = 0;
    };

    // ── Pre-built Layouts ────────────────────────────────────

    namespace VertexLayouts
    {
        // Empty layout for fullscreen passes using SV_VertexID (no vertex buffer)
        inline constexpr VertexLayout Empty = {
            .stride = 0,
            .attribs = nullptr,
            .attribCount = 0,
        };

        inline constexpr std::array<VertexAttrib, 1> PosAttribs = {{
            {0, offsetof(VertexPos, Position), VertexAttribFormat::Float3},
        }};

        inline constexpr VertexLayout Pos = {
            .stride = sizeof(VertexPos),
            .attribs = PosAttribs.data(),
            .attribCount = static_cast<uint32_t>(PosAttribs.size()),
        };

        inline constexpr std::array<VertexAttrib, 2> PosColourAttribs = {{
            {0, offsetof(VertexPosColour, Position), VertexAttribFormat::Float3},
            {1, offsetof(VertexPosColour, Colour),    VertexAttribFormat::Float3},
        }};

        inline constexpr VertexLayout PosColour = {
            .stride = sizeof(VertexPosColour),
            .attribs = PosColourAttribs.data(),
            .attribCount = static_cast<uint32_t>(PosColourAttribs.size()),
        };

        inline constexpr std::array<VertexAttrib, 3> PosNormalUVAttribs = {{
            {0, offsetof(VertexPosNormalUV, Position), VertexAttribFormat::Float3},
            {1, offsetof(VertexPosNormalUV, Normal),   VertexAttribFormat::Float3},
            {2, offsetof(VertexPosNormalUV, UV),       VertexAttribFormat::Float2},
        }};

        inline constexpr VertexLayout PosNormalUV = {
            .stride = sizeof(VertexPosNormalUV),
            .attribs = PosNormalUVAttribs.data(),
            .attribCount = static_cast<uint32_t>(PosNormalUVAttribs.size()),
        };

        inline constexpr std::array<VertexAttrib, 3> PosNormalColourAttribs = {{
            {0, offsetof(VertexPosNormalColour, Position), VertexAttribFormat::Float3},
            {1, offsetof(VertexPosNormalColour, Normal),   VertexAttribFormat::Float3},
            {2, offsetof(VertexPosNormalColour, Colour),    VertexAttribFormat::Float3},
        }};

        inline constexpr VertexLayout PosNormalColour = {
            .stride = sizeof(VertexPosNormalColour),
            .attribs = PosNormalColourAttribs.data(),
            .attribCount = static_cast<uint32_t>(PosNormalColourAttribs.size()),
        };
    }

} // namespace Wayfinder
