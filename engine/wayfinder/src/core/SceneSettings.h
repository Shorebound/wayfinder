#pragma once

#include "wayfinder_exports.h"

#include <optional>
#include <string>
#include <string_view>

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
        toml::table Data;

        /// Get a typed value by key. Returns nullopt if absent or wrong type.
        template <typename T>
        std::optional<T> Get(std::string_view key) const { return Data[key].template value<T>(); }

        /// Get a typed value or a default.
        template <typename T>
        T GetOr(std::string_view key, T defaultValue) const { return Data[key].template value_or(std::move(defaultValue)); }

        /// Set or overwrite a value at runtime (override).
        template <typename T>
        void Set(const std::string& key, T&& value) { Data.insert_or_assign(key, std::forward<T>(value)); }
    };

} // namespace Wayfinder
