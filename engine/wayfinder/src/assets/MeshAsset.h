#pragma once

#include "AssetLoader.h"
#include "MeshFormat.h"
#include "core/Identifiers.h"
#include "wayfinder_exports.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Wayfinder
{
    struct MeshSubmeshDescriptor
    {
        std::string Name;
        uint32_t MaterialSlot = 0;
    };

    /**
     * @brief CPU-side mesh asset loaded from a JSON descriptor + .wfmesh binary.
     */
    struct WAYFINDER_API MeshAsset
    {
        AssetId Id;
        std::string Name;

        /// Relative path to the .wfmesh file (from the JSON descriptor's directory).
        std::string SourcePath;

        std::vector<MeshSubmeshDescriptor> SubmeshDescriptors;

        /// Parsed geometry — one entry per submesh in the binary.
        std::vector<SubmeshCpuData> Submeshes;

        AxisAlignedBounds Bounds{};

        void ReleaseGeometryData();
    };

    WAYFINDER_API bool ValidateMeshAssetDocument(const nlohmann::json& document, const std::filesystem::path& filePath, std::string& error);

    /** @brief Validate the referenced .wfmesh binary exists and passes layout checks. */
    WAYFINDER_API bool ValidateMeshAssetBinary(const nlohmann::json& document, const std::filesystem::path& filePath, std::string& error);

    WAYFINDER_API std::optional<MeshAsset> LoadMeshAssetFromDocument(const nlohmann::json& document, const std::filesystem::path& filePath, std::string& error);

    template<>
    struct AssetLoader<MeshAsset>
    {
        static std::optional<MeshAsset> Load(const nlohmann::json& document, const std::filesystem::path& filePath, std::string& error)
        {
            return LoadMeshAssetFromDocument(document, filePath, error);
        }
    };

} // namespace Wayfinder
