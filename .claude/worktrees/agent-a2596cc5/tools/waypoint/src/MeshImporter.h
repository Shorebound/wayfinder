#pragma once

#include "core/Result.h"

#include <filesystem>
#include <string_view>

namespace Wayfinder::Waypoint
{
    struct MeshImportRequest
    {
        std::filesystem::path SourcePath;
        std::filesystem::path OutputDirectory;
        std::string_view NameStem;
    };

    /**
     * @brief Import a glTF/GLB file into a mesh.json + .wfmesh pair under outputDir.
     *
     * @param request Import request containing the source path, output directory,
     *                and optional output stem.
     * @return Success, or an error describing the failure.
     */
    Result<void> ImportMesh(const MeshImportRequest& request);

} // namespace Wayfinder::Waypoint
