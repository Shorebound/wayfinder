#pragma once

#include "core/Result.h"

#include <filesystem>
#include <string_view>

namespace Wayfinder::Waypoint
{
    /**
     * @brief Import a glTF/GLB file into a mesh.json + .wfmesh pair under outputDir.
     *
     * @param gltfPath   Path to .gltf or .glb
     * @param outputDir  Directory to write outputs (created if missing)
     * @param nameStem   Base file name for outputs (without extension)
     * @return Success, or an error describing the failure.
     */
    Result<void> ImportMesh(const std::filesystem::path& gltfPath, const std::filesystem::path& outputDir, std::string_view nameStem);

} // namespace Wayfinder::Waypoint
