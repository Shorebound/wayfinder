#pragma once

/**
 * @brief Engine-level profiling macros.
 *
 * Thin wrappers over Tracy that compile to no-ops when profiling is disabled.
 * All instrumentation call sites use these macros — never raw Tracy symbols —
 * so the profiler back-end can be swapped in a single header.
 */

#ifdef WAYFINDER_PROFILING_ENABLED
#include <tracy/Tracy.hpp>

/// RAII zone covering the enclosing scope (unnamed).
#define WAYFINDER_PROFILE_SCOPE() ZoneScoped
/// RAII zone covering the enclosing scope with a static display name.
#define WAYFINDER_PROFILE_SCOPE_NAMED(name) ZoneScopedN(name)
/// Alias for WAYFINDER_PROFILE_SCOPE() — reads well at function level.
#define WAYFINDER_PROFILE_FUNCTION() ZoneScoped

/// Mark the end of a logical frame (drives Tracy's frame-time graph).
#define WAYFINDER_PROFILE_FRAME_MARK() FrameMark
/// Emit a named numeric value for Tracy's plot view.
#define WAYFINDER_PROFILE_PLOT(name, value) TracyPlot(name, value)

#else
#define WAYFINDER_PROFILE_SCOPE() ((void)0)
#define WAYFINDER_PROFILE_SCOPE_NAMED(name) ((void)0)
#define WAYFINDER_PROFILE_FUNCTION() ((void)0)

#define WAYFINDER_PROFILE_FRAME_MARK() ((void)0)
#define WAYFINDER_PROFILE_PLOT(name, value) ((void)0)
#endif
