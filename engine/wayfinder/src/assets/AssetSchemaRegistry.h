#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <string_view>

#include <toml++/toml.hpp>

#include "AssetRegistry.h"
#include "../graphics/Material.h"

namespace Wayfinder
{
    class AssetSchemaRegistry
    {
    public:
        static bool ValidateDocument(
            AssetKind kind,
            const toml::table& document,
            const std::filesystem::path& filePath,
            std::string& error)
        {
            switch (kind)
            {
            case AssetKind::Prefab:
                return ValidatePrefabDocument(document, filePath, error);
            case AssetKind::Material:
                return ValidateMaterialDocument(document, filePath, error);
            case AssetKind::Unknown:
                error = "Asset '" + filePath.generic_string() + "' resolved to unknown asset kind";
                return false;
            }

            error = "Asset '" + filePath.generic_string() + "' resolved to unsupported asset kind";
            return false;
        }

    private:
        static bool ValidatePrefabDocument(
            const toml::table& document,
            const std::filesystem::path& filePath,
            std::string& error)
        {
            constexpr std::array<std::string_view, 3> metadataKeys = {
                "asset_id",
                "asset_type",
                "name"
            };

            for (const auto& [key, node] : document)
            {
                const std::string_view keyView = key.str();
                bool isMetadata = false;
                for (const std::string_view metadataKey : metadataKeys)
                {
                    if (keyView == metadataKey)
                    {
                        isMetadata = true;
                        break;
                    }
                }

                if (isMetadata)
                {
                    continue;
                }

                if (!node.is_table())
                {
                    error = "Prefab asset '" + filePath.generic_string() + "' field '" + std::string{keyView}
                        + "' must be a TOML table. Prefab payload is defined by component tables.";
                    return false;
                }
            }

            return true;
        }

        static bool ValidateMaterialDocument(
            const toml::table& document,
            const std::filesystem::path& filePath,
            std::string& error)
        {
            MaterialAsset material;
            return ParseMaterialAssetDocument(document, filePath.generic_string(), material, error);
        }
    };
} // namespace Wayfinder