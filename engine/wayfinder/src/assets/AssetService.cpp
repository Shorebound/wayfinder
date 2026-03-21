#include "AssetService.h"

namespace Wayfinder
{
    bool AssetService::SetAssetRoot(const std::filesystem::path& assetRoot, std::string& error)
    {
        const std::filesystem::path normalisedRoot = assetRoot.empty() ? std::filesystem::path{} : std::filesystem::weakly_canonical(assetRoot);
        if (normalisedRoot == m_assetRoot)
        {
            return true;
        }

        m_assetRoot = normalisedRoot;
        m_hasAssetRegistry = false;
        m_materialAssetsById.clear();
        m_missingMaterialAssets.clear();

        if (m_assetRoot.empty())
        {
            return true;
        }

        return (m_hasAssetRegistry = m_assetRegistry.BuildFromDirectory(m_assetRoot, error));
    }

    const AssetRecord* AssetService::ResolveRecord(const AssetId& assetId) const
    {
        return m_hasAssetRegistry ? m_assetRegistry.ResolveRecord(assetId) : nullptr;
    }

    const std::filesystem::path* AssetService::ResolvePath(const AssetId& assetId) const
    {
        return m_hasAssetRegistry ? m_assetRegistry.ResolvePath(assetId) : nullptr;
    }

    const MaterialAsset* AssetService::LoadMaterialAsset(const AssetId& assetId, std::string& error)
    {
        if (!m_hasAssetRegistry)
        {
            error = "Asset registry is not initialised for the current asset root.";
            return nullptr;
        }

        if (const auto cached = m_materialAssetsById.find(assetId); cached != m_materialAssetsById.end())
        {
            return &cached->second;
        }

        if (m_missingMaterialAssets.find(assetId) != m_missingMaterialAssets.end())
        {
            error = "Material asset '" + assetId.ToString() + "' could not be resolved from the active asset root.";
            return nullptr;
        }

        const std::filesystem::path* materialPath = m_assetRegistry.ResolvePath(assetId);
        if (!materialPath)
        {
            m_missingMaterialAssets.emplace(assetId, true);
            error = "Material asset '" + assetId.ToString() + "' is not registered under the active asset root.";
            return nullptr;
        }

        MaterialAsset materialAsset;
        if (!LoadMaterialAssetFromFile(*materialPath, materialAsset, error))
        {
            m_missingMaterialAssets.emplace(assetId, true);
            return nullptr;
        }

        const auto [it, inserted] = m_materialAssetsById.emplace(materialAsset.Id, std::move(materialAsset));
        return &it->second;
    }
} // namespace Wayfinder
