#include "ProjectResolver.h"
#include "core/Log.h"
#include "core/Result.h"

namespace Wayfinder
{

    Result<std::filesystem::path> FindProjectFile()
    {
        std::error_code ec;
        const auto cwd = std::filesystem::current_path(ec);
        if (ec)
        {
            WAYFINDER_WARNING(LogEngine, "Failed to resolve current working directory: {}", ec.message());
            return MakeError("Failed to resolve current working directory");
        }
        return FindProjectFile(cwd);
    }

    Result<std::filesystem::path> FindProjectFile(const std::filesystem::path& startPath)
    {
        std::error_code ec;
        std::filesystem::path searchDir = startPath;

        // If startPath is a file, walk up from its parent directory.
        if (std::filesystem::is_regular_file(searchDir, ec))
        {
            searchDir = searchDir.parent_path();
        }
        else if (ec)
        {
            WAYFINDER_WARNING(LogEngine, "Failed to stat start path '{}': {}", startPath.string(), ec.message());
            return MakeError("Failed to stat start path");
        }

        // Canonicalise so the walk-up terminates predictably.
        ec.clear();
        const auto canonical = std::filesystem::weakly_canonical(searchDir, ec);
        if (ec)
        {
            WAYFINDER_WARNING(LogEngine, "Failed to canonicalise start path '{}': {}", searchDir.string(), ec.message());
            return MakeError("Failed to canonicalise start path");
        }

        searchDir = canonical;

        while (true)
        {
            const auto candidate = searchDir / kProjectFileName;

            ec.clear();
            const bool exists = std::filesystem::exists(candidate, ec);
            if (ec)
            {
                WAYFINDER_WARNING(LogEngine, "Failed to query existence of project file candidate '{}': {}", candidate.string(), ec.message());
                return MakeError("Failed to query project file candidate");
            }

            if (exists)
            {
                WAYFINDER_INFO(LogEngine, "Found project file: {}", candidate.string());
                return candidate;
            }

            const auto parent = searchDir.parent_path();
            if (parent == searchDir) break; // reached filesystem root

            searchDir = parent;
        }

        return MakeError("No project.wayfinder found");
    }

    std::optional<std::filesystem::path> FindAssetRoot(const std::filesystem::path& startPath)
    {
        std::error_code ec;
        std::filesystem::path searchDir = startPath;

        if (std::filesystem::is_regular_file(searchDir, ec))
        {
            searchDir = searchDir.parent_path();
        }
        else if (ec)
        {
            return std::nullopt;
        }

        ec.clear();
        const auto canonical = std::filesystem::weakly_canonical(searchDir, ec);
        if (ec)
        {
            return std::nullopt;
        }

        searchDir = canonical;

        while (true)
        {
            if (searchDir.filename() == "assets")
            {
                return searchDir;
            }

            const auto parent = searchDir.parent_path();
            if (parent == searchDir) break;

            searchDir = parent;
        }

        return std::nullopt;
    }

} // namespace Wayfinder
