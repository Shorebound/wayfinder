#pragma once

#include "core/Result.h"
#include "plugins/IRegistrar.h"

#include <toml++/toml.hpp>

#include <filesystem>
#include <span>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Wayfinder
{

    /**
     * @brief Build-time registrar tracking declared config types and TOML file keys.
     *
     * Plugins declare config types via AppBuilder::LoadConfig<T>(key),
     * which delegates here. Provides 3-tier TOML loading with caching:
     * struct defaults < config/<key>.toml < saved/config/<key>.toml.
     */
    class ConfigRegistrar : public IRegistrar
    {
    public:
        struct ConfigEntry
        {
            std::string Key;
            std::type_index Type;
        };

        /// Declare a config type associated with a TOML file key.
        void DeclareConfig(std::string_view key, std::type_index type)
        {
            m_entries.push_back(ConfigEntry{std::string(key), type});
        }

        /// Load and cache a TOML table with 3-tier layering for the given key.
        /// Struct defaults < config/<key>.toml < saved/config/<key>.toml.
        /// Returns pointer to the cached merged table.
        [[nodiscard]] auto LoadTable(std::string_view key, const std::filesystem::path& configDir, const std::filesystem::path& savedDir) -> Result<const toml::table*>;

        /// Get all declared config entries.
        [[nodiscard]] auto GetEntries() const -> std::span<const ConfigEntry>
        {
            return m_entries;
        }

    private:
        std::vector<ConfigEntry> m_entries;
        std::unordered_map<std::string, toml::table> m_tableCache;
    };

} // namespace Wayfinder
