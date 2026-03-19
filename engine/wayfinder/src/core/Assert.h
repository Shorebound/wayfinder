#pragma once

#include "Log.h"

#include <cstdlib>

/// Engine assertion that logs a fatal message and terminates in all builds.
/// Unlike C assert(), this is never compiled out.
///
/// Usage:
///   WAYFINDER_ASSERT(ptr != nullptr, "Expected non-null pointer");
///   WAYFINDER_ASSERT(index < size, "Index {} out of range {}", index, size);
#define WAYFINDER_ASSERT(condition, ...)                                              \
    do                                                                                \
    {                                                                                 \
        if (!(condition)) [[unlikely]]                                                \
        {                                                                             \
            WAYFINDER_FATAL(Wayfinder::LogEngine, "ASSERT FAILED: " __VA_ARGS__);     \
            std::abort();                                                             \
        }                                                                             \
    } while (false)
