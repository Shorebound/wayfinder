#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include "AssetRegistry.h"
#include "../rendering/Material.h"

namespace Wayfinder
{
    class WAYFINDER_API AssetService
    {
    public:
        bool SetAssetRoot(const std::filesystem::path& assetRoot, std::string& error);

        const std::filesystem::path& GetAssetRoot() const { return m_assetRoot; }
        const AssetRecord* ResolveRecord(const AssetId& assetId) const;
        const std::filesystem::path* ResolvePath(const AssetId& assetId) const;
        const MaterialAsset* LoadMaterialAsset(const AssetId& assetId, std::string& error);

    private:
        std::filesystem::path m_assetRoot;
        AssetRegistry m_assetRegistry;
        bool m_hasAssetRegistry = false;
        std::unordered_map<AssetId, MaterialAsset> m_materialAssetsById;
        std::unordered_map<AssetId, bool> m_missingMaterialAssets;
    };
} // namespace Wayfinder