#pragma once

#include <cstdint>
#include <cstring>

namespace Wayfinder
{
    // 64-bit sort key layout:
    //   Opaque / Overlay: [2 bits: layer] [6 bits: blend group] [16 bits: material ID] [32 bits: depth] [8 bits: sub-sort]
    //   Transparent:      [2 bits: layer] [32 bits: depth] [6 bits: blend group] [16 bits: material ID] [8 bits: sub-sort]
    //
    // Layer: 0 = Opaque, 1 = Transparent, 2 = Overlay
    // Depth: camera-space Z encoded as uint32. Opaques sort front-to-back (smaller Z first),
    //        transparents sort back-to-front (larger Z first) — achieved by flipping depth bits for transparent.
    // Blend group: compact bucket for compatible blend states. Transparent keys place this after depth so
    //        back-to-front ordering always wins over state clustering.
    // Sub-sort: 8-bit tiebreaker for deterministic ordering (render priority today).

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

        inline uint64_t Build(SortLayer layer, uint8_t blendGroup, uint16_t materialId, float cameraSpaceZ, uint8_t subSort)
        {
            uint64_t key = 0;

            // [63:62] — 2-bit layer
            key |= (static_cast<uint64_t>(layer) & 0x3) << 62;

            uint32_t depthBits = DepthToUint32(cameraSpaceZ);
            if (layer == SortLayer::Transparent)
            {
                depthBits = ~depthBits;
            }

            if (layer == SortLayer::Transparent)
            {
                // [61:30] — 32-bit depth
                key |= static_cast<uint64_t>(depthBits) << 30;

                // [29:24] — 6-bit blend group
                key |= static_cast<uint64_t>(blendGroup & 0x3F) << 24;

                // [23:8] — 16-bit material ID
                key |= static_cast<uint64_t>(materialId) << 8;
            }
            else
            {
                // [61:56] — 6-bit blend group
                key |= static_cast<uint64_t>(blendGroup & 0x3F) << 56;

                // [55:40] — 16-bit material ID
                key |= static_cast<uint64_t>(materialId) << 40;

                // [39:8] — 32-bit depth
                key |= static_cast<uint64_t>(depthBits) << 8;
            }

            // [7:0] — 8-bit sub-sort tiebreaker
            key |= static_cast<uint64_t>(subSort);

            return key;
        }
    }

} // namespace Wayfinder
