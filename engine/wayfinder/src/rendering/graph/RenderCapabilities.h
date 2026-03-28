#pragma once

#include <cstdint>

namespace Wayfinder
{
    /// Bitmask describing what a `RenderFeature` injector may do. Used for documentation, dev validation in
    /// `RenderGraph::Compile` when declared on the builder, and future scheduling (compute queues, profiling buckets).
    namespace RenderCapabilities
    {
        inline constexpr uint32_t RASTER = 1u << 0;
        /// Scene mesh submissions (opaque / forward-style geometry).
        inline constexpr uint32_t RASTER_SCENE_GEOMETRY = 1u << 1;
        /// Overlays, debug draws, non-scene raster work.
        inline constexpr uint32_t RASTER_OVERLAY_OR_DEBUG = 1u << 2;
        /// Fullscreen composite / post-style injectors (reserved).
        inline constexpr uint32_t FULLSCREEN_COMPOSITE = 1u << 3;
        /// Compute graph nodes (reserved).
        inline constexpr uint32_t COMPUTE = 1u << 4;
    }

    using RenderCapabilityMask = uint32_t;

} // namespace Wayfinder
