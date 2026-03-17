#pragma once

#include <string_view>

#include <flecs.h>
#include <toml++/toml.hpp>

#include "wayfinder_exports.h"

namespace Wayfinder
{
    class Entity;

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

        void RegisterComponents(flecs::world& world) const;
        void ApplyComponents(const toml::table& componentTables, Entity& entity) const;
        void SerializeComponents(const Entity& entity, toml::table& componentTables) const;
        bool ValidateComponent(std::string_view key, const toml::table& componentTable, std::string& error) const;
        bool IsRegistered(std::string_view key) const;

    private:
        const Entry* Find(std::string_view key) const;
    };
} // namespace Wayfinder