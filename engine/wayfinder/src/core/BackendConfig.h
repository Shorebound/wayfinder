#pragma once

#include "wayfinder_exports.h"

namespace Wayfinder
{
    enum class PlatformBackend
    {
        SDL3,
    };

    enum class RenderBackend
    {
        SDL_GPU,
        Null,
    };

    struct WAYFINDER_API BackendConfig
    {
        PlatformBackend Platform = PlatformBackend::SDL3;
        RenderBackend Rendering = RenderBackend::SDL_GPU;
    };
} // namespace Wayfinder