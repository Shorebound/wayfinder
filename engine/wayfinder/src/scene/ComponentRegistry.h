#pragma once

#include <span>
#include <string_view>

#include "ecs/Flecs.h"
#include <nlohmann/json.hpp>

#include "wayfinder_exports.h"

namespace Wayfinder
{
    class Entity;

    /**
     * @brief Static compile-time registry of core serialisable ECS components.
     *
     * This holds the engine's built-in component entries. At runtime,
     * RuntimeComponentRegistry merges these with game-registered entries.
     */
    class WAYFINDER_API SceneComponentRegistry
    {
    public:
        struct Entry
        {
            std::string_view Key;
            void (*RegisterFn)(flecs::world& world);
            void (*ApplyFn)(const nlohmann::json& componentData, Entity& entity);
            void (*SerialiseFn)(const Entity& entity, nlohmann::json& componentTables);
            bool (*ValidateFn)(const nlohmann::json& componentData, std::string& error);
        };

        static const SceneComponentRegistry& Get();

        /// Access the raw core entries (used by RuntimeComponentRegistry to seed itself).
        static std::span<const Entry> GetEntries();

        void RegisterComponents(flecs::world& world) const;
        void ApplyComponents(const nlohmann::json& componentTables, Entity& entity) const;
        void SerialiseComponents(const Entity& entity, nlohmann::json& componentTables) const;
        bool ValidateComponent(std::string_view key, const nlohmann::json& componentData, std::string& error) const;
        bool IsRegistered(std::string_view key) const;

    private:
        const Entry* Find(std::string_view key) const;
    };
} // namespace Wayfinder