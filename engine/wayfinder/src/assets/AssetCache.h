#pragma once

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "AssetLoader.h"
#include "AssetRegistry.h"
#include "core/Log.h"

namespace Wayfinder
{
    /**
     * @brief Generic typed asset cache.
     *
     * Stores loaded assets of type TAsset keyed by AssetId, with miss tracking
     * to avoid re-attempting failed loads. Requires an AssetLoader<TAsset>
     * specialisation for the actual parsing.
     *
     * @tparam TAsset The asset type to cache. Must be movable.
     */
    template<typename TAsset>
    class AssetCache
    {
    public:
        /// Retrieve a previously loaded asset (cache hit only).
        const TAsset* Get(const AssetId& assetId) const
        {
            if (const auto it = m_assets.find(assetId); it != m_assets.end())
            {
                return &it->second;
            }
            return nullptr;
        }

        /**
         * @brief Load an asset from the registry or return it from cache.
         *
         * On cache miss, resolves the path through the registry, reads the JSON
         * file, and delegates to AssetLoader<TAsset>::Load for parsing. Caches
         * the result on success, records the miss on failure.
         */
        const TAsset* LoadOrGet(const AssetId& assetId, const AssetRegistry& registry, std::string& error)
        {
            // Cache hit
            if (const auto it = m_assets.find(assetId); it != m_assets.end())
            {
                return &it->second;
            }

            // Known miss — avoid re-attempting
            if (m_misses.contains(assetId))
            {
                error = "Asset '" + assetId.ToString() + "' was previously unresolvable.";
                return nullptr;
            }

            // Resolve path
            const std::filesystem::path* path = registry.ResolvePath(assetId);
            if (!path)
            {
                m_misses.emplace(assetId, true);
                error = "Asset '" + assetId.ToString() + "' is not registered under the active asset root.";
                return nullptr;
            }

            // Read and parse JSON
            nlohmann::json document;
            try
            {
                std::ifstream file(path->string());
                if (!file.is_open())
                {
                    m_misses.emplace(assetId, true);
                    error = "Failed to open asset file '" + path->generic_string() + "'";
                    return nullptr;
                }
                document = nlohmann::json::parse(file);
            }
            catch (const nlohmann::json::exception& parseError)
            {
                m_misses.emplace(assetId, true);
                error = "Failed to parse asset '" + path->generic_string() + "': " + parseError.what();
                return nullptr;
            }

            // Delegate to the typed loader
            std::optional<TAsset> loaded = AssetLoader<TAsset>::Load(document, *path, error);
            if (!loaded)
            {
                m_misses.emplace(assetId, true);
                return nullptr;
            }

            const auto [it, inserted] = m_assets.emplace(assetId, std::move(*loaded));
            return &it->second;
        }

        /// Clear all cached assets and miss records.
        void Clear()
        {
            m_assets.clear();
            m_misses.clear();
        }

        /// Number of successfully cached assets.
        size_t Size() const { return m_assets.size(); }

    private:
        std::unordered_map<AssetId, TAsset> m_assets;
        std::unordered_map<AssetId, bool> m_misses;
    };

} // namespace Wayfinder
