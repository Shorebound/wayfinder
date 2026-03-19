#pragma once

#include "BackendConfig.h"
#include <cstdint>
#include <filesystem>
#include <string>

namespace Wayfinder
{

    struct WindowConfig
    {
        uint32_t Width = 1920;
        uint32_t Height = 1080;
        std::string Title = "Wayfinder Engine";
        bool VSync = false;
    };

    struct ShaderConfig
    {
        std::string Directory = "assets/shaders";
    };

    struct WAYFINDER_API EngineConfig
    {
        WindowConfig Window;
        BackendConfig Backends;
        ShaderConfig Shaders;

        static EngineConfig LoadFromFile(const std::filesystem::path& path);
        static EngineConfig LoadDefaults();
    };

} // namespace Wayfinder
