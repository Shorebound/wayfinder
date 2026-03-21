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
        m_materialCache.Clear();
        m_textureCache.Clear();

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

        return m_materialCache.LoadOrGet(assetId, m_assetRegistry, error);
    }
} // namespace Wayfinder
