#include "AssetRegistry.h"

#include "AssetSchemaRegistry.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace Wayfinder
{
    namespace
    {
        const std::string kAssetIdKey = "asset_id";
        const std::string kAssetTypeKey = "asset_type";
        const std::string kNameKey = "name";
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
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
            {
                continue;
            }

            try
            {
                std::ifstream file(entry.path().string());
                if (!file.is_open())
                {
                    continue;
                }
                nlohmann::json document = nlohmann::json::parse(file);

                if (!document.contains(kAssetIdKey) || !document[kAssetIdKey].is_string())
                {
                    continue;
                }

                const std::string assetIdText = document[kAssetIdKey].get<std::string>();
                const std::optional<AssetId> assetId = AssetId::Parse(assetIdText);
                if (!assetId)
                {
                    error = "Invalid asset_id '" + assetIdText + "' found while scanning '" + rootDirectory.generic_string() + "'";
                    return false;
                }

                if (!document.contains(kAssetTypeKey) || !document[kAssetTypeKey].is_string())
                {
                    error = "Asset '" + entry.path().generic_string() + "' is missing required field 'asset_type'.";
                    return false;
                }

                const std::string assetTypeText = document[kAssetTypeKey].get<std::string>();

                if (!AssetSchemaRegistry::ValidateDocument(assetTypeText, document, entry.path(), error))
                {
                    return false;
                }

                const std::optional<AssetKind> parsedAssetKind = AssetSchemaRegistry::ResolveBuiltinKind(assetTypeText);

                const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(entry.path());
                if (m_assetRecordsById.find(*assetId) != m_assetRecordsById.end())
                {
                    error = "Duplicate asset_id '" + assetId->ToString() + "' found while scanning '" + rootDirectory.generic_string() + "'";
                    return false;
                }

                AssetRecord record;
                record.Id = *assetId;
                record.TypeName = assetTypeText;
                record.Kind = parsedAssetKind.value_or(AssetKind::Unknown);
                record.Path = canonicalPath;
                record.Name = document.value(std::string{kNameKey}, entry.path().stem().string());
                m_assetRecordsById.emplace(*assetId, std::move(record));
            }
            catch (const nlohmann::json::exception&)
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
        case AssetKind::Texture:
            return "texture";
        case AssetKind::Unknown:
            return "unknown";
        }

        return "unknown";
    }
} // namespace Wayfinder