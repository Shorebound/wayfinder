#pragma once

#include "wayfinder_exports.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <toml++/toml.hpp>

namespace Wayfinder
{
    /** @struct SceneSettings
     *  @brief World singleton holding the active scene's authored settings.
     *
     *  Loaded from the [settings] table in a scene TOML file and set as an ECS
     *  singleton when the scene loads. Systems read it via world.get<SceneSettings>().
     *  Data is stored as a generic toml::table so scenes can define arbitrary
     *  key-value settings without engine-side schema changes.
     */
    struct WAYFINDER_API SceneSettings
    {
        /// Get a typed scalar value by key. Returns nullopt if absent or wrong type.
        /// @note Only scalar types (int, double, bool, std::string, etc.) are supported.
        ///       For arrays or composite types, use GetData() and query the toml::table directly.
        template <typename T>
        std::optional<T> Get(std::string_view key) const { return m_data[key].template value<T>(); }

        /// Get a typed scalar value or a default.
        /// @note Only scalar types are supported. For arrays or composite types, use GetData().
        template <typename T>
        T GetOr(std::string_view key, T defaultValue) const { return m_data[key].template value_or(std::move(defaultValue)); }

        /// Set or overwrite a scalar value at runtime (override).
        /// @note Only scalar types are supported. For arrays or composite types, mutate via GetData().
        template <typename T>
        void Set(const std::string& key, T&& value) { m_data.insert_or_assign(key, std::forward<T>(value)); }

        /// Read-only access to the underlying table.
        const toml::table& GetData() const { return m_data; }

        /// Replace the entire settings table (used during scene load).
        void SetData(toml::table data) { m_data = std::move(data); }

    private:
        toml::table m_data;
    };

} // namespace Wayfinder
