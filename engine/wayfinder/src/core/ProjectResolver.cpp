#include "ProjectResolver.h"
#include "Log.h"

namespace Wayfinder
{

    std::optional<std::filesystem::path> FindProjectFile(const std::filesystem::path& startPath)
    {
        std::filesystem::path searchDir = startPath;

        if (std::filesystem::is_regular_file(searchDir))
            searchDir = searchDir.parent_path();

        // Canonicalise so the walk-up terminates predictably.
        searchDir = std::filesystem::weakly_canonical(searchDir);

        while (true)
        {
            const auto candidate = searchDir / kProjectFileName;
            if (std::filesystem::exists(candidate))
            {
                WAYFINDER_INFO(LogEngine, "Found project file: {}", candidate.string());
                return candidate;
            }

            const auto parent = searchDir.parent_path();
            if (parent == searchDir)
                break; // reached filesystem root

            searchDir = parent;
        }

        return std::nullopt;
    }

} // namespace Wayfinder
