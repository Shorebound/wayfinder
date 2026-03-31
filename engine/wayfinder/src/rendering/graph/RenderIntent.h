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
        inline const InternedString MAIN = InternedString::Intern("main");
        inline const InternedString OVERLAY = InternedString::Intern("overlay");
    }

    namespace FrameLayerIds
    {
        inline const InternedString MAIN_SCENE = InternedString::Intern("main_scene");
        inline const InternedString OVERLAY_SCENE = InternedString::Intern("overlay_scene");
        inline const InternedString DEBUG = InternedString::Intern("debug");
    }
} // namespace Wayfinder
