#pragma once

#include "platform/BackendConfig.h"
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
        std::string SourceDirectory; ///< .slang source root for runtime compilation (empty = disabled)
    };

    /**
     * @brief Configuration for physics simulation timestepping.
     *
     * The physics world advances in fixed increments of FixedTimestep,
     * accumulating leftover frame time between frames.
     */
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
