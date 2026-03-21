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

    struct PhysicsConfig
    {
        /// Fixed simulation timestep in seconds.  The physics world advances
        /// in increments of this size, accumulating leftover frame time.
        float FixedTimestep = 1.0f / 60.0f;
    };

    struct WAYFINDER_API EngineConfig
    {
        WindowConfig Window;
        BackendConfig Backends;
        ShaderConfig Shaders;
        PhysicsConfig Physics;

        static EngineConfig LoadFromFile(const std::filesystem::path& path);
        static EngineConfig LoadDefaults();
    };

} // namespace Wayfinder
