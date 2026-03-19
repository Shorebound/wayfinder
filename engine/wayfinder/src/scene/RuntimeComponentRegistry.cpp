#include "RuntimeComponentRegistry.h"
#include "ComponentRegistry.h"
#include "../core/ModuleRegistry.h"

namespace Wayfinder
{
    void RuntimeComponentRegistry::AddCoreEntries()
    {
        for (const auto& coreEntry : SceneComponentRegistry::GetEntries())
        {
            Entry entry;
            entry.Key = std::string(coreEntry.Key);
            entry.RegisterFn = coreEntry.RegisterFn;
            entry.ApplyFn = coreEntry.ApplyFn;
            entry.SerializeFn = coreEntry.SerializeFn;
            entry.ValidateFn = coreEntry.ValidateFn;
            m_entries.push_back(std::move(entry));
        }
    }

    void RuntimeComponentRegistry::AddGameEntries(const ModuleRegistry& registry)
    {
        for (const auto& desc : registry.GetComponentDescriptors())
        {
            Entry entry;
            entry.Key = desc.Key;
            entry.RegisterFn = desc.RegisterFn;
            entry.ApplyFn = desc.ApplyFn;
            entry.SerializeFn = desc.SerializeFn;
            entry.ValidateFn = desc.ValidateFn;
            m_entries.push_back(std::move(entry));
        }
    }

    void RuntimeComponentRegistry::RegisterComponents(flecs::world& world) const
    {
        for (const Entry& entry : m_entries)
        {
            if (entry.RegisterFn)
                entry.RegisterFn(world);
        }
    }

    void RuntimeComponentRegistry::ApplyComponents(const toml::table& componentTables, Entity& entity) const
    {
        for (const auto& [key, node] : componentTables)
        {
            const Entry* entry = Find(key.str());
            if (!entry || !entry->ApplyFn)
                continue;

            const toml::table* componentTable = node.as_table();
            if (!componentTable)
                continue;

            entry->ApplyFn(*componentTable, entity);
        }
    }

    void RuntimeComponentRegistry::SerializeComponents(const Entity& entity, toml::table& componentTables) const
    {
        for (const Entry& entry : m_entries)
        {
            if (entry.SerializeFn)
                entry.SerializeFn(entity, componentTables);
        }
    }

    bool RuntimeComponentRegistry::ValidateComponent(std::string_view key, const toml::table& componentTable, std::string& error) const
    {
        const Entry* entry = Find(key);
        if (!entry || !entry->ValidateFn)
        {
            error = "component is not registered for scene authoring";
            return false;
        }

        return entry->ValidateFn(componentTable, error);
    }

    bool RuntimeComponentRegistry::IsRegistered(std::string_view key) const
    {
        const Entry* entry = Find(key);
        return entry != nullptr && entry->ApplyFn != nullptr && entry->ValidateFn != nullptr;
    }

    const RuntimeComponentRegistry::Entry* RuntimeComponentRegistry::Find(std::string_view key) const
    {
        for (const Entry& entry : m_entries)
        {
            if (entry.Key == key)
                return &entry;
        }

        return nullptr;
    }
} // namespace Wayfinder
