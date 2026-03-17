#include "AssetRegistry.h"

#include "AssetSchemaRegistry.h"

#include <toml++/toml.hpp>

namespace Wayfinder
{
    namespace
    {
        constexpr std::string_view kAssetIdKey = "asset_id";
        constexpr std::string_view kAssetTypeKey = "asset_type";
        constexpr std::string_view kNameKey = "name";
    }

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

                const auto assetTypeText = document[kAssetTypeKey].value<std::string>();
                if (!assetTypeText)
                {
                    error = "Asset '" + entry.path().generic_string() + "' is missing required field 'asset_type'.";
                    return false;
                }

                if (!AssetSchemaRegistry::ValidateDocument(*assetTypeText, document, entry.path(), error))
                {
                    return false;
                }

                const std::optional<AssetKind> parsedAssetKind = AssetSchemaRegistry::ResolveBuiltinKind(*assetTypeText);

                const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(entry.path());
                if (m_assetRecordsById.find(*assetId) != m_assetRecordsById.end())
                {
                    error = "Duplicate asset_id '" + assetId->ToString() + "' found while scanning '" + rootDirectory.generic_string() + "'";
                    return false;
                }

                AssetRecord record;
                record.Id = *assetId;
                record.TypeName = *assetTypeText;
                record.Kind = parsedAssetKind.value_or(AssetKind::Unknown);
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

    std::optional<AssetKind> AssetRegistry::ParseKind(const std::string_view text)
    {
        return AssetSchemaRegistry::ResolveBuiltinKind(text);
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