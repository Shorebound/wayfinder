#pragma once

#include <cstdint>

namespace Wayfinder
{
    /// Bitmask describing what a `RenderPass` injector may do. Used for documentation, dev validation,
    /// and future scheduling (compute queues, profiling buckets).
    namespace RenderPassCapabilities
    {
        inline constexpr uint32_t Raster = 1u << 0;
        /// Scene mesh submissions (opaque / forward-style geometry).
        inline constexpr uint32_t RasterSceneGeometry = 1u << 1;
        /// Overlays, debug draws, non-scene raster work.
        inline constexpr uint32_t RasterOverlayOrDebug = 1u << 2;
        /// Fullscreen composite / post-style injectors (reserved).
        inline constexpr uint32_t FullscreenComposite = 1u << 3;
        /// Compute graph nodes (reserved).
        inline constexpr uint32_t Compute = 1u << 4;
    }

    using RenderPassCapabilityMask = uint32_t;

} // namespace Wayfinder
