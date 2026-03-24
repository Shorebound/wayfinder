#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ecs/Flecs.h"
#include <nlohmann/json.hpp>

#include "wayfinder_exports.h"

namespace Wayfinder::Plugins
{
    class PluginRegistry;
}

namespace Wayfinder
{
    class Entity;

    /**
     * @brief Unified registry of serialisable ECS components, merging engine-core
     * entries with game-registered entries from PluginRegistry.
     *
     * Built once by Game::InitialiseWorld() and passed by const-reference
     * to Scene, SceneDocument, and validation tools.
     *
     * @par Lifecycle & Thread Safety
     * Populated during Game::InitialiseWorld() via AddCoreEntries() and
     * AddGameEntries() (which mutate m_entries). After initialisation the
     * registry is accessed only through const methods (Find, IsRegistered,
     * RegisterComponents, ApplyComponents, SerialiseComponents,
     * ValidateComponent) and is therefore safe for concurrent read-only
     * access. Callers must not invoke mutating methods after initialisation
     * unless external synchronisation is provided.
     */
    class WAYFINDER_API RuntimeComponentRegistry
    {
    public:
        struct Entry
        {
            std::string Key;
            void (*RegisterFn)(flecs::world& world);
            void (*ApplyFn)(const nlohmann::json& componentData, Entity& entity);
            void (*SerialiseFn)(const Entity& entity, nlohmann::json& componentTables);
            bool (*ValidateFn)(const nlohmann::json& componentData, std::string& error);
        };

        /// Seed with core entries (from SceneComponentRegistry).
        void AddCoreEntries();

        /// Merge game entries from a PluginRegistry's component descriptors.
        void AddGameEntries(const Plugins::PluginRegistry& registry);

        /// Register all components into a flecs world.
        void RegisterComponents(flecs::world& world) const;

        void ApplyComponents(const nlohmann::json& componentTables, Entity& entity) const;
        void SerialiseComponents(const Entity& entity, nlohmann::json& componentTables) const;
        bool ValidateComponent(std::string_view key, const nlohmann::json& componentData, std::string& error) const;
        bool IsRegistered(std::string_view key) const;

    private:
        const Entry* Find(std::string_view key) const;

        std::vector<Entry> m_entries;
        std::unordered_map<std::string, size_t> m_index; ///< Key -> index into m_entries for O(1) lookup.
    };
} // namespace Wayfinder
