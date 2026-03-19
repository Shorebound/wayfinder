#pragma once

#include "wayfinder_exports.h"

#include <filesystem>
#include <optional>

namespace Wayfinder
{

    /// Filename used to identify a Wayfinder project root directory.
    inline constexpr const char* kProjectFileName = "project.wayfinder";

    /// Walks the directory tree upward from `startPath` looking for a
    /// `project.wayfinder` file. Returns the path to that file if found, or
    /// nullopt if the filesystem root is reached without finding one.
    ///
    /// If `startPath` is a file, the search begins from its parent directory.
    WAYFINDER_API std::optional<std::filesystem::path> FindProjectFile(
        const std::filesystem::path& startPath = std::filesystem::current_path());

} // namespace Wayfinder
