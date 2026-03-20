#pragma once

#include "core/InternedString.h"

namespace Wayfinder
{
    using RenderLayerId = InternedString;
    using RenderPassId = InternedString;

    namespace RenderLayers
    {
        inline const InternedString Main = InternedString::Intern("main");
        inline const InternedString Overlay = InternedString::Intern("overlay");
    }

    namespace RenderPassIds
    {
        inline const InternedString MainScene = InternedString::Intern("main_scene");
        inline const InternedString OverlayScene = InternedString::Intern("overlay_scene");
        inline const InternedString Debug = InternedString::Intern("debug");
    }
} // namespace Wayfinder