#include "app/ConfigRegistrar.h"

#include "core/Log.h"

#include <format>

namespace
{

    /// Shallow-recursive merge: override values replace base values at each level.
    /// Sub-tables are merged recursively. Arrays are replaced entirely.
    void MergeToml(toml::table& base, const toml::table& overrides)
    {
        for (auto&& [key, node] : overrides)
        {
            if (node.is_value())
            {
                if (auto v_int = node.value<int64_t>())
                {
                    base.insert_or_assign(key, *v_int);
                }
                else if (auto v_double = node.value<double>())
                {
                    base.insert_or_assign(key, *v_double);
                }
                else if (auto v_str = node.value<std::string>())
                {
                    base.insert_or_assign(key, *v_str);
                }
                else if (auto v_bool = node.value<bool>())
                {
                    base.insert_or_assign(key, *v_bool);
                }
            }
            else if (const auto* subTable = node.as_table())
            {
                if (auto* existing = base[key].as_table())
                {
                    MergeToml(*existing, *subTable);
                }
            }
        }
    }

} // namespace

namespace Wayfinder
{

    auto ConfigRegistrar::LoadTable(std::string_view key, const std::filesystem::path& configDir, const std::filesystem::path& savedDir) -> Result<const toml::table*>
    {
        const std::string keyStr(key);
        if (auto it = m_tableCache.find(keyStr); it != m_tableCache.end())
        {
            return &it->second;
        }

        toml::table merged;

        // Layer 2: config/<key>.toml (project defaults)
        const auto configPath = configDir / std::format("{}.toml", key);
        if (std::filesystem::exists(configPath))
        {
            try
            {
                merged = toml::parse_file(configPath.string());
            }
            catch (const toml::parse_error& ex)
            {
                return MakeError(std::format("Failed to parse {}: {}", configPath.string(), ex.what()));
            }
        }
        else
        {
            Log::Info(LogEngine, "No config/{}.toml found, using defaults", key);
        }

        // Layer 3: saved/config/<key>.toml (user overrides)
        const auto savedPath = savedDir / "config" / std::format("{}.toml", key);
        if (std::filesystem::exists(savedPath))
        {
            try
            {
                const auto overrides = toml::parse_file(savedPath.string());
                MergeToml(merged, overrides);
                Log::Info(LogEngine, "Applied user overrides from saved/config/{}.toml", key);
            }
            catch (const toml::parse_error& ex)
            {
                return MakeError(std::format("Failed to parse {}: {}", savedPath.string(), ex.what()));
            }
        }

        auto [it, inserted] = m_tableCache.emplace(keyStr, std::move(merged));
        return &it->second;
    }

} // namespace Wayfinder
