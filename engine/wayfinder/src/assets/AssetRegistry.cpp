#include "AssetRegistry.h"

#include <toml++/toml.hpp>

namespace
{
    constexpr std::string_view kAssetIdKey = "asset_id";
    constexpr std::string_view kAssetTypeKey = "asset_type";
    constexpr std::string_view kNameKey = "name";

    std::optional<Wayfinder::AssetKind> ParseAssetKind(std::string_view text)
    {
        if (text == "prefab")
        {
            return Wayfinder::AssetKind::Prefab;
        }

        if (text == "material")
        {
            return Wayfinder::AssetKind::Material;
        }

        return std::nullopt;
    }

    Wayfinder::AssetKind InferAssetKind(const toml::table& document)
    {
        if (const auto assetType = document[kAssetTypeKey].value<std::string>())
        {
            if (const auto parsed = ParseAssetKind(*assetType))
            {
                return *parsed;
            }

            return Wayfinder::AssetKind::Unknown;
        }

        if (document.contains("transform")
            || document.contains("mesh")
            || document.contains("camera")
            || document.contains("light")
            || document.contains("material"))
        {
            return Wayfinder::AssetKind::Prefab;
        }

        if (document.contains("base_color") || document.contains("wireframe"))
        {
            return Wayfinder::AssetKind::Material;
        }

        return Wayfinder::AssetKind::Unknown;
    }
}

namespace Wayfinder
{
    bool AssetRegistry::BuildFromDirectory(const std::filesystem::path& rootDirectory, std::string& error)
    {
        m_assetRecordsById.clear();

        if (!std::filesystem::exists(rootDirectory))
        {
            return true;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(rootDirectory))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".toml")
            {
                continue;
            }

            try
            {
                toml::table document = toml::parse_file(entry.path().string());
                const auto assetIdText = document[kAssetIdKey].value<std::string>();
                if (!assetIdText)
                {
                    continue;
                }

                const std::optional<AssetId> assetId = AssetId::Parse(*assetIdText);
                if (!assetId)
                {
                    error = "Invalid asset_id '" + *assetIdText + "' found while scanning '" + rootDirectory.generic_string() + "'";
                    return false;
                }

                const AssetKind assetKind = InferAssetKind(document);
                if (assetKind == AssetKind::Unknown)
                {
                    error = "Could not determine asset type for '" + entry.path().generic_string() + "'. Add asset_type = 'prefab' or 'material'.";
                    return false;
                }

                const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(entry.path());
                if (m_assetRecordsById.find(*assetId) != m_assetRecordsById.end())
                {
                    error = "Duplicate asset_id '" + assetId->ToString() + "' found while scanning '" + rootDirectory.generic_string() + "'";
                    return false;
                }

                AssetRecord record;
                record.Id = *assetId;
                record.Kind = assetKind;
                record.Path = canonicalPath;
                record.Name = document[kNameKey].value_or(entry.path().stem().string());
                m_assetRecordsById.emplace(*assetId, std::move(record));
            }
            catch (const toml::parse_error&)
            {
                continue;
            }
        }

        return true;
    }

    const std::filesystem::path* AssetRegistry::ResolvePath(const AssetId& assetId) const
    {
        const AssetRecord* record = ResolveRecord(assetId);
        if (!record)
        {
            return nullptr;
        }

        return &record->Path;
    }

    const AssetRecord* AssetRegistry::ResolveRecord(const AssetId& assetId) const
    {
        const auto it = m_assetRecordsById.find(assetId);
        if (it == m_assetRecordsById.end())
        {
            return nullptr;
        }

        return &it->second;
    }

    std::string_view AssetRegistry::ToString(const AssetKind kind)
    {
        switch (kind)
        {
        case AssetKind::Prefab:
            return "prefab";
        case AssetKind::Material:
            return "material";
        case AssetKind::Unknown:
            return "unknown";
        }

        return "unknown";
    }
} // namespace Wayfinder