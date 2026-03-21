#pragma once

#include "Result.h"
#include "wayfinder_exports.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Wayfinder
{

    struct ProjectPaths
    {
        std::string AssetRoot = "assets";
        std::string BootScene = "scenes/default_scene.toml"; // relative to AssetRoot
        std::string ConfigDir = "config";
        std::string Module;  ///< Shared library name for game module (empty = none).
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
        std::filesystem::path ResolveAssetRoot() const { return ProjectRoot / Paths.AssetRoot; }

        /// Resolve the absolute path to the boot scene.
        std::filesystem::path ResolveBootScene() const { return ResolveAssetRoot() / Paths.BootScene; }

        /// Resolve the absolute path to the config directory.
        std::filesystem::path ResolveConfigDir() const { return ProjectRoot / Paths.ConfigDir; }

        /// Resolve the absolute path to engine.toml.
        std::filesystem::path ResolveEngineConfigPath() const { return ResolveConfigDir() / "engine.toml"; }

        /// Resolve the absolute path to the game module shared library.
        /// Applies platform-specific prefix/suffix (e.g. .dll on Windows, lib*.so on Linux).
        /// Returns empty path if no module is configured.
        std::filesystem::path ResolveModulePath() const;

        /// Load a project descriptor from a file with validation.
        /// On success, returns a ProjectLoadOutput containing the descriptor and
        /// any warnings encountered during parsing/validation.
        /// On failure, returns an Error describing the problem.
        static Result<struct ProjectLoadOutput> LoadFromFile(const std::filesystem::path& path);
    };

    /// Successful output of loading a project descriptor.
    /// Contains the parsed descriptor and any non-fatal warnings.
    struct WAYFINDER_API ProjectLoadOutput
    {
        ProjectDescriptor Descriptor;
        std::vector<std::string> Warnings;
    };

} // namespace Wayfinder
