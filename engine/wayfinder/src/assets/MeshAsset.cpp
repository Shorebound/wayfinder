#include "MeshAsset.h"

#include "core/Log.h"

#include <fstream>
#include <iterator>
#include <span>

namespace Wayfinder
{
    namespace
    {
        constexpr std::string_view ASSET_ID_KEY = "asset_id";
        constexpr std::string_view ASSET_TYPE_KEY = "asset_type";
        constexpr std::string_view NAME_KEY = "name";
        constexpr std::string_view SOURCE_KEY = "source";
        constexpr std::string_view SUBMESHES_KEY = "submeshes";
    }

    void MeshAsset::ReleaseGeometryData()
    {
        for (SubmeshCpuData& sm : Submeshes)
        {
            sm.VertexBytes.clear();
            sm.VertexBytes.shrink_to_fit();
            sm.IndexBytes.clear();
            sm.IndexBytes.shrink_to_fit();
        }
    }

    bool ValidateMeshAssetDocument(const nlohmann::json& document, const std::filesystem::path& filePath, std::string& error)
    {
        const std::string label = filePath.generic_string();

        if (!document.contains(ASSET_ID_KEY) || !document.at(ASSET_ID_KEY).is_string())
        {
            error = "Mesh asset '" + label + "' is missing asset_id";
            return false;
        }

        if (!document.contains(ASSET_TYPE_KEY) || !document.at(ASSET_TYPE_KEY).is_string())
        {
            error = "Mesh asset '" + label + "' is missing asset_type";
            return false;
        }

        if (document.at(ASSET_TYPE_KEY).get<std::string>() != "mesh")
        {
            error = "Mesh asset '" + label + "' must declare asset_type = 'mesh'";
            return false;
        }

        if (!document.contains(SOURCE_KEY) || !document.at(SOURCE_KEY).is_string())
        {
            error = "Mesh asset '" + label + "' is missing 'source' — the path to the .wfmesh file";
            return false;
        }

        if (document.contains(SUBMESHES_KEY))
        {
            const auto& submeshesNode = document.at(SUBMESHES_KEY);
            if (!submeshesNode.is_array())
            {
                error = "Mesh asset '" + label + "' field 'submeshes' must be an array";
                return false;
            }

            for (size_t i = 0; i < submeshesNode.size(); ++i)
            {
                const auto& entry = submeshesNode.at(i);
                if (!entry.is_object())
                {
                    error = "Mesh asset '" + label + "' submeshes[" + std::to_string(i) + "] must be an object";
                    return false;
                }

                if (!entry.contains("name") || !entry.at("name").is_string())
                {
                    error = "Mesh asset '" + label + "' submeshes[" + std::to_string(i) + "] requires string 'name'";
                    return false;
                }

                if (entry.contains("material_slot") && !entry.at("material_slot").is_number_unsigned())
                {
                    error = "Mesh asset '" + label + "' submeshes[" + std::to_string(i) + "] 'material_slot' must be a non-negative integer";
                    return false;
                }
            }
        }

        return true;
    }

    bool ValidateMeshAssetBinary(const nlohmann::json& document, const std::filesystem::path& filePath, std::string& error)
    {
        if (!document.contains(SOURCE_KEY) || !document.at(SOURCE_KEY).is_string())
        {
            error = "Mesh asset binary validation requires 'source'";
            return false;
        }

        const std::filesystem::path meshDir = filePath.parent_path();
        const std::filesystem::path binaryPath = meshDir / document.at(SOURCE_KEY).get<std::string>();

        if (!std::filesystem::exists(binaryPath))
        {
            error = "Mesh binary '" + binaryPath.generic_string() + "' does not exist";
            return false;
        }

        std::ifstream file(binaryPath.string(), std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            error = "Failed to open mesh binary '" + binaryPath.generic_string() + "'";
            return false;
        }

        const auto fileSize = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        std::vector<std::byte> bytes(fileSize);
        if (fileSize > 0)
        {
            file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(fileSize));
        }

        return ValidateMeshBinaryLayout(bytes, error);
    }

    std::optional<MeshAsset> LoadMeshAssetFromDocument(const nlohmann::json& document, const std::filesystem::path& filePath, std::string& error)
    {
        const std::string label = filePath.generic_string();

        if (!ValidateMeshAssetDocument(document, filePath, error))
        {
            return std::nullopt;
        }

        const std::string assetIdText = document.at(ASSET_ID_KEY).get<std::string>();
        const std::optional<AssetId> assetId = AssetId::Parse(assetIdText);
        if (!assetId)
        {
            error = "Mesh asset '" + label + "' has an invalid asset_id";
            return std::nullopt;
        }

        MeshAsset mesh;
        mesh.Id = *assetId;
        mesh.Name = document.value(std::string{NAME_KEY}, filePath.stem().string());
        mesh.SourcePath = document.at(SOURCE_KEY).get<std::string>();

        if (document.contains(SUBMESHES_KEY))
        {
            for (const auto& entry : document.at(SUBMESHES_KEY))
            {
                MeshSubmeshDescriptor desc;
                desc.Name = entry.at("name").get<std::string>();
                desc.MaterialSlot = entry.value("material_slot", 0u);
                mesh.SubmeshDescriptors.push_back(std::move(desc));
            }
        }

        const std::filesystem::path meshDir = filePath.parent_path();
        const std::filesystem::path binaryPath = meshDir / mesh.SourcePath;

        if (!std::filesystem::exists(binaryPath))
        {
            error = "Mesh asset '" + label + "' references binary '" + binaryPath.generic_string() + "' which does not exist";
            return std::nullopt;
        }

        std::ifstream file(binaryPath.string(), std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            error = "Failed to open mesh binary '" + binaryPath.generic_string() + "'";
            return std::nullopt;
        }

        const auto fileSize = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        std::vector<std::byte> bytes(fileSize);
        if (fileSize > 0)
        {
            file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(fileSize));
        }

        ParsedMeshFile parsed;
        if (!ParseMeshFile(bytes, parsed, error))
        {
            return std::nullopt;
        }

        mesh.Bounds = parsed.Header.Bounds;
        mesh.Submeshes = std::move(parsed.Submeshes);

        WAYFINDER_INFO(LogAssets, "Loaded mesh '{}' ({} submeshes) from '{}'", mesh.Name, mesh.Submeshes.size(), binaryPath.generic_string());

        return mesh;
    }

} // namespace Wayfinder
