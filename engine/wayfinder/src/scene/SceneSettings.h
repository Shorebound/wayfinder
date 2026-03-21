#pragma once

#include "wayfinder_exports.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

namespace Wayfinder
{
    /** @struct SceneSettings
     *  @brief World singleton holding the active scene's authored settings.
     *
     *  Loaded from the "settings" object in a scene JSON file and set as an ECS
     *  singleton when the scene loads. Systems read it via world.get<SceneSettings>().
     *  Data is stored as a generic JSON object so scenes can define arbitrary
     *  key-value settings without engine-side schema changes.
     */
    struct WAYFINDER_API SceneSettings
    {
        /// Get a typed scalar value by key. Returns nullopt if absent or wrong type.
        template <typename T>
        std::optional<T> Get(std::string_view key) const
        {
            const std::string k{key};
            auto it = m_data.find(k);
            if (it == m_data.end()) return std::nullopt;
            try { return it->get<T>(); }
            catch (...) { return std::nullopt; }
        }

        /// Get a typed scalar value or a default.
        template <typename T>
        T GetOr(std::string_view key, T defaultValue) const
        {
            const std::string k{key};
            auto it = m_data.find(k);
            if (it == m_data.end()) return defaultValue;
            try { return it->get<T>(); }
            catch (...) { return defaultValue; }
        }

        /// Set or overwrite a scalar value at runtime (override).
        template <typename T>
        void Set(const std::string& key, T&& value) { m_data[key] = std::forward<T>(value); }

        /// Read-only access to the underlying data.
        const nlohmann::json& GetData() const { return m_data; }

        /// Mutable access to the underlying data for in-place edits.
        nlohmann::json& GetData() { return m_data; }

        /// Replace the entire settings data (used during scene load).
        void SetData(nlohmann::json data) { m_data = std::move(data); }

    private:
        nlohmann::json m_data = nlohmann::json::object();
    };

} // namespace Wayfinder
