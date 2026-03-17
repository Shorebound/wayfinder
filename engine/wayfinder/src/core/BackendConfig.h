#pragma once

#include "wayfinder_exports.h"

namespace Wayfinder
{
    enum class PlatformBackend
    {
        Raylib,
    };

    enum class RenderBackend
    {
        Raylib,
    };

    struct WAYFINDER_API BackendConfig
    {
        PlatformBackend Platform = PlatformBackend::Raylib;
        RenderBackend Rendering = RenderBackend::Raylib;
    };
} // namespace Wayfinder