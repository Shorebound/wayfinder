#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include "AssetCache.h"
#include "AssetRegistry.h"
#include "TextureAsset.h"
#include "rendering/materials/Material.h"

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

        /// Generic typed asset access — delegates to the appropriate AssetCache<T>.
        template<typename TAsset>
        const TAsset* LoadAsset(const AssetId& assetId, std::string& error);

        /// Mutable access to a cached asset (e.g. to release pixel data after GPU upload).
        template<typename TAsset>
        TAsset* GetMutableAsset(const AssetId& assetId);

        const AssetRegistry& GetRegistry() const { return m_assetRegistry; }

    private:
        std::filesystem::path m_assetRoot;
        AssetRegistry m_assetRegistry;
        bool m_hasAssetRegistry = false;

        AssetCache<MaterialAsset> m_materialCache;
        AssetCache<TextureAsset> m_textureCache;
    };

    // ── Template implementation ──────────────────────────────

    template<>
    inline const MaterialAsset* AssetService::LoadAsset<MaterialAsset>(const AssetId& assetId, std::string& error)
    {
        return LoadMaterialAsset(assetId, error);
    }

    template<>
    inline const TextureAsset* AssetService::LoadAsset<TextureAsset>(const AssetId& assetId, std::string& error)
    {
        if (!m_hasAssetRegistry)
        {
            error = "Asset registry is not initialised for the current asset root.";
            return nullptr;
        }

        return m_textureCache.LoadOrGet(assetId, m_assetRegistry, error);
    }

    template<>
    inline TextureAsset* AssetService::GetMutableAsset<TextureAsset>(const AssetId& assetId)
    {
        return m_textureCache.GetMutable(assetId);
    }

} // namespace Wayfinder