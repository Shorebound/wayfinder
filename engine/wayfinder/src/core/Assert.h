#pragma once

#include "Log.h"

#include <cstdlib>

/// Engine assertion that logs a fatal message and terminates in all builds.
/// Unlike C assert(), this is never compiled out.
///
/// Kept as a macro for string-literal concatenation ("ASSERT FAILED: " fmt)
/// and because the condition must not be evaluated when assertions are stripped
/// in a future build configuration.
///
/// Usage:
///   WAYFINDER_ASSERT(ptr != nullptr, "Expected non-null pointer");
///   WAYFINDER_ASSERT(index < size, "Index {} out of range {}", index, size);

#define WAYFINDER_ASSERT(condition, fmt, ...)                                                                                                                                                                              \
    if (!!(condition)) [[likely]]                                                                                                                                                                                          \
    {                                                                                                                                                                                                                      \
    }                                                                                                                                                                                                                      \
    else                                                                                                                                                                                                                   \
    {                                                                                                                                                                                                                      \
        Wayfinder::Log::Fatal(Wayfinder::LogEngine, "ASSERT FAILED: " fmt __VA_OPT__(, ) __VA_ARGS__);                                                                                                                     \
        std::abort();                                                                                                                                                                                                      \
    }