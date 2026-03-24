#pragma once

#include "core/Result.h"
#include "wayfinder_exports.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Wayfinder
{

    struct ProjectPaths
    {
        std::string AssetRoot = "assets";
        /// Path to the boot scene, relative to AssetRoot.
        std::string BootScene = "scenes/default_scene.json";
        std::string ConfigDir = "config";
        /// Base name of the game plugin shared library (no prefix/suffix; empty = none).
        std::string Plugin;
    };

    inline constexpr const char* DEFAULT_PROJECT_NAME = "Untitled";

    struct WAYFINDER_API ProjectDescriptor
    {
        std::string Name = DEFAULT_PROJECT_NAME;
        std::string Version = "0.1.0";
        std::string EngineVersion = "0.1.0";

        ProjectPaths Paths;

        /// The directory containing project.wayfinder (set after loading).
        std::filesystem::path ProjectRoot;

        /// Resolve the absolute path to the asset root.
        std::filesystem::path ResolveAssetRoot() const
        {
            return ProjectRoot / Paths.AssetRoot;
        }

        /// Resolve the absolute path to the boot scene.
        std::filesystem::path ResolveBootScene() const
        {
            return ResolveAssetRoot() / Paths.BootScene;
        }

        /// Resolve the absolute path to the config directory.
        std::filesystem::path ResolveConfigDir() const
        {
            return ProjectRoot / Paths.ConfigDir;
        }

        /// Resolve the absolute path to engine.toml.
        std::filesystem::path ResolveEngineConfigPath() const
        {
            return ResolveConfigDir() / "engine.toml";
        }

        /// Resolve the absolute path to the game plugin shared library.
        /// Applies platform-specific prefix/suffix (e.g. .dll on Windows, lib*.so on Linux).
        /// Returns empty path if no plugin library is configured.
        std::filesystem::path ResolvePluginLibraryPath() const;

        /**
         * @brief Load a project descriptor from a TOML file with validation.
         * @param path  Path to the `project.wayfinder` file.
         * @return On success a ProjectLoadOutput containing the parsed
         *         descriptor and any non-fatal warnings.  On failure an Error
         *         describing the problem (file not found, parse error, etc.).
         */
        static Result<struct ProjectLoadOutput> LoadFromFile(const std::filesystem::path& path);
    };

    /**
     * @brief Successful output of loading a project descriptor.
     *
     * Contains the parsed ProjectDescriptor and any non-fatal warnings
     * encountered during parsing or post-parse validation.
     */
    struct WAYFINDER_API ProjectLoadOutput
    {
        ProjectDescriptor Descriptor;
        std::vector<std::string> Warnings;
    };

} // namespace Wayfinder
