#pragma once

#include <string>
#include <string_view>

namespace Wayfinder
{
    using RenderLayerId = std::string;
    using RenderPassId = std::string;

    namespace RenderLayers
    {
        inline constexpr std::string_view Main = "main";
        inline constexpr std::string_view Overlay = "overlay";
    }

    namespace RenderPassIds
    {
        inline constexpr std::string_view MainScene = "main_scene";
        inline constexpr std::string_view OverlayScene = "overlay_scene";
        inline constexpr std::string_view Debug = "debug";
    }
} // namespace Wayfinder