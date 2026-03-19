#pragma once

#include "wayfinder_exports.h"

#include <filesystem>
#include <string>

namespace Wayfinder
{

    struct ProjectPaths
    {
        std::string AssetRoot = "assets";
        std::string BootScene = "scenes/default_scene.toml"; // relative to AssetRoot
        std::string ConfigDir = "config";
    };

    struct WAYFINDER_API ProjectDescriptor
    {
        std::string Name = "Untitled";
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

        static ProjectDescriptor LoadFromFile(const std::filesystem::path& path);
    };

} // namespace Wayfinder
