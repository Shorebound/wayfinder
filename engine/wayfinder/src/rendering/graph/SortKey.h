#pragma once

#include <cstdint>
#include <cstring>

namespace Wayfinder
{
    // 64-bit sort key layout:
    //   [2 bits: layer]  [16 bits: pipeline/material ID]  [32 bits: depth]  [14 bits: sub-sort]
    //
    // Layer: 0 = Opaque, 1 = Transparent, 2 = Overlay
    // Depth: camera-space Z encoded as uint32. Opaques sort front-to-back (smaller Z first),
    //        transparents sort back-to-front (larger Z first) — achieved by flipping depth bits for transparent.
    // Sub-sort: tiebreaker for deterministic ordering (entity hash or sequence counter).

    enum class SortLayer : uint8_t
    {
        Opaque = 0,
        Transparent = 1,
        Overlay = 2,
    };

    namespace SortKeyBuilder
    {
        inline uint32_t DepthToUint32(float cameraSpaceZ)
        {
            // Camera-space Z is negative (in front of camera). Negate to get positive distance.
            float depth = -cameraSpaceZ;
            if (depth < 0.0f)
            {
                depth = 0.0f;
            }

            // Reinterpret as uint32 for bitwise sorting.
            // IEEE 754 floats sort correctly when positive (same bit ordering).
            uint32_t bits = 0;
            std::memcpy(&bits, &depth, sizeof(bits));
            return bits;
        }

        inline uint64_t Build(SortLayer layer, uint16_t materialId, float cameraSpaceZ, uint16_t subSort)
        {
            uint64_t key = 0;

            // [63:62] — 2-bit layer
            key |= (static_cast<uint64_t>(layer) & 0x3) << 62;

            // [61:46] — 16-bit pipeline/material ID
            key |= static_cast<uint64_t>(materialId) << 46;

            // [45:14] — 32-bit depth
            uint32_t depthBits = DepthToUint32(cameraSpaceZ);

            // For transparent layer, flip depth so back-to-front sorts naturally as ascending key order
            if (layer == SortLayer::Transparent)
            {
                depthBits = ~depthBits;
            }

            key |= static_cast<uint64_t>(depthBits) << 14;

            // [13:0] — 14-bit sub-sort tiebreaker
            key |= static_cast<uint64_t>(subSort & 0x3FFF);

            return key;
        }
    }

} // namespace Wayfinder
