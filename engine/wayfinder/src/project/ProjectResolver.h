#pragma once

#include "core/Result.h"
#include "wayfinder_exports.h"

#include <filesystem>

namespace Wayfinder
{
    /// Filename used to identify a Wayfinder project root directory.
    inline constexpr std::string_view PROJECT_FILE_NAME = "project.wayfinder";

    /**
     * @brief Walk the directory tree upward from @p startPath looking for
     *        a `project.wayfinder` file.
     *
     * If @p startPath is a regular file the search begins from its parent
     * directory.
     *
     * @param startPath  Directory (or file) to begin the upward search from.
     * @return On success the absolute path to the discovered
     *         `project.wayfinder` file.  On failure an Error describing
     *         the cause — filesystem stat/canonicalisation failures,
     *         existence-query errors, or reaching the filesystem root
     *         without finding the file.
     */
    WAYFINDER_API Result<std::filesystem::path> FindProjectFile(const std::filesystem::path& startPath);

    /**
     * @brief Convenience overload that begins the search from the current
     *        working directory.
     *
     * Resolves the CWD via the non-throwing `std::filesystem::current_path`
     * overload and forwards to the single-argument FindProjectFile.
     *
     * @return See FindProjectFile(const std::filesystem::path&) for details.
     */
    WAYFINDER_API Result<std::filesystem::path> FindProjectFile();

    /// Walks the directory tree upward from `startPath` looking for a
    /// directory named `assets`. Returns the path to that directory if found,
    /// or nullopt if the filesystem root is reached without finding one.
    ///
    /// If `startPath` is a file, the search begins from its parent directory.
    WAYFINDER_API std::optional<std::filesystem::path> FindAssetRoot(const std::filesystem::path& startPath);

} // namespace Wayfinder
