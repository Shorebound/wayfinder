#pragma once

#include <span>
#include <string_view>

#include <flecs.h>
#include <toml++/toml.hpp>

#include "wayfinder_exports.h"

namespace Wayfinder
{
    class Entity;

    /// Static compile-time registry of core serializable ECS components.
    ///
    /// This holds the engine's built-in component entries. At runtime,
    /// RuntimeComponentRegistry merges these with game-registered entries.
    class WAYFINDER_API SceneComponentRegistry
    {
    public:
        struct Entry
        {
            std::string_view Key;
            void (*RegisterFn)(flecs::world& world);
            void (*ApplyFn)(const toml::table& componentTable, Entity& entity);
            void (*SerializeFn)(const Entity& entity, toml::table& componentTables);
            bool (*ValidateFn)(const toml::table& componentTable, std::string& error);
        };

        static const SceneComponentRegistry& Get();

        /// Access the raw core entries (used by RuntimeComponentRegistry to seed itself).
        static std::span<const Entry> GetEntries();

        void RegisterComponents(flecs::world& world) const;
        void ApplyComponents(const toml::table& componentTables, Entity& entity) const;
        void SerializeComponents(const Entity& entity, toml::table& componentTables) const;
        bool ValidateComponent(std::string_view key, const toml::table& componentTable, std::string& error) const;
        bool IsRegistered(std::string_view key) const;

    private:
        const Entry* Find(std::string_view key) const;
    };
} // namespace Wayfinder