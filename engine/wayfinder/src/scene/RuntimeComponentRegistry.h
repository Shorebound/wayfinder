#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <flecs.h>
#include <toml++/toml.hpp>

#include "wayfinder_exports.h"

namespace Wayfinder
{
    class Entity;

    /// Unified registry of serializable ECS components, merging engine-core
    /// entries with game-registered entries from ModuleRegistry.
    ///
    /// Built once by Game::InitializeWorld() and passed by const-reference
    /// to Scene, SceneDocument, and validation tools.
    class WAYFINDER_API RuntimeComponentRegistry
    {
    public:
        struct Entry
        {
            std::string Key;
            void (*RegisterFn)(flecs::world& world);
            void (*ApplyFn)(const toml::table& componentTable, Entity& entity);
            void (*SerializeFn)(const Entity& entity, toml::table& componentTables);
            bool (*ValidateFn)(const toml::table& componentTable, std::string& error);
        };

        /// Seed with core entries (from SceneComponentRegistry).
        void AddCoreEntries();

        /// Merge game entries from a ModuleRegistry's component descriptors.
        void AddGameEntries(const class ModuleRegistry& registry);

        /// Register all components into a flecs world.
        void RegisterComponents(flecs::world& world) const;

        void ApplyComponents(const toml::table& componentTables, Entity& entity) const;
        void SerializeComponents(const Entity& entity, toml::table& componentTables) const;
        bool ValidateComponent(std::string_view key, const toml::table& componentTable, std::string& error) const;
        bool IsRegistered(std::string_view key) const;

    private:
        const Entry* Find(std::string_view key) const;

        std::vector<Entry> m_entries;
    };
} // namespace Wayfinder
