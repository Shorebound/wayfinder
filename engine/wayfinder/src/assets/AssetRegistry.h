#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

#include "../core/Identifiers.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    enum class AssetKind
    {
        Unknown,
        Prefab,
        Material
    };

    struct WAYFINDER_API AssetRecord
    {
        AssetId Id;
        AssetKind Kind = AssetKind::Unknown;
        std::filesystem::path Path;
        std::string Name;
    };

    class WAYFINDER_API AssetRegistry
    {
    public:
        bool BuildFromDirectory(const std::filesystem::path& rootDirectory, std::string& error);

        const std::filesystem::path* ResolvePath(const AssetId& assetId) const;
        const AssetRecord* ResolveRecord(const AssetId& assetId) const;

        static std::string_view ToString(AssetKind kind);

    private:
        std::unordered_map<AssetId, AssetRecord> m_assetRecordsById;
    };
} // namespace Wayfinder