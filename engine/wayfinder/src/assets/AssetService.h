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

        /**
         * @brief Generic typed asset access — delegates to the appropriate AssetCache<T>.
         *
         * @param assetId  The asset identifier to load.
         * @param error    Populated with a description on failure.
         * @return Pointer to the cached asset, or nullptr on failure.
         */
        template<typename TAsset>
        const TAsset* LoadAsset(const AssetId& assetId, std::string& error);

        /**
         * @brief Release CPU-side pixel data for a cached texture asset.
         *
         * Call after the texture has been uploaded to the GPU to free the
         * in-memory pixel buffer.  The asset remains in the cache (metadata
         * intact) but its pixel data will be empty afterwards.
         *
         * @param assetId  The asset ID of the texture whose pixel data should be released.
         */
        void ReleaseTexturePixelData(const AssetId& assetId);

        /**
         * @brief Invalidate a cached texture asset, forcing a reload on next access.
         *
         * Use when the cached texture has stale data (e.g. pixel data was released)
         * and needs to be re-read from disk.
         *
         * @param assetId  The asset ID of the texture to evict from cache.
         */
        void InvalidateTextureAsset(const AssetId& assetId);

        /** @brief Return the active asset registry. */
        const AssetRegistry& GetRegistry() const { return m_assetRegistry; }

    private:
        /// Mutable access to a cached asset — restricted to internal use.
        template<typename TAsset>
        TAsset* GetMutableAsset(const AssetId& assetId);

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