#pragma once

#include "core/InternedString.h"

namespace Wayfinder
{
    /// Scene sorting group (e.g. main vs overlay), not the logical frame layer id.
    using RenderGroupId = InternedString;

    /// Identifies a CPU-side frame layer (`FrameLayer`), e.g. main_scene vs debug.
    using FrameLayerId = InternedString;

    namespace RenderGroups
    {
        inline const InternedString Main = InternedString::Intern("main");
        inline const InternedString Overlay = InternedString::Intern("overlay");
    }

    namespace FrameLayerIds
    {
        inline const InternedString MainScene = InternedString::Intern("main_scene");
        inline const InternedString OverlayScene = InternedString::Intern("overlay_scene");
        inline const InternedString Debug = InternedString::Intern("debug");
    }
} // namespace Wayfinder
