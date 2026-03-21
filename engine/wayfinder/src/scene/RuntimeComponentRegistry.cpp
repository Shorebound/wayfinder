#include "RuntimeComponentRegistry.h"
#include "ComponentRegistry.h"
#include "core/Log.h"
#include "modules/ModuleRegistry.h"

namespace Wayfinder
{
    void RuntimeComponentRegistry::AddCoreEntries()
    {
        for (const auto& coreEntry : SceneComponentRegistry::GetEntries())
        {
            if (Find(coreEntry.Key))
            {
                WAYFINDER_LOG(LogScene, LogVerbosity::Warning,
                    "Duplicate component key '{}' in core entries, skipping", coreEntry.Key);
                continue;
            }

            Entry entry;
            entry.Key = std::string(coreEntry.Key);
            entry.RegisterFn = coreEntry.RegisterFn;
            entry.ApplyFn = coreEntry.ApplyFn;
            entry.SerialiseFn = coreEntry.SerialiseFn;
            entry.ValidateFn = coreEntry.ValidateFn;
            m_entries.push_back(std::move(entry));
            m_index[m_entries.back().Key] = m_entries.size() - 1;
        }
    }

    void RuntimeComponentRegistry::AddGameEntries(const ModuleRegistry& registry)
    {
        for (const auto& desc : registry.GetComponentDescriptors())
        {
            if (Find(desc.Key))
            {
                WAYFINDER_LOG(LogScene, LogVerbosity::Warning,
                    "Duplicate component key '{}' from game module, skipping", desc.Key);
                continue;
            }

            Entry entry;
            entry.Key = desc.Key;
            entry.RegisterFn = desc.RegisterFn;
            entry.ApplyFn = desc.ApplyFn;
            entry.SerialiseFn = desc.SerialiseFn;
            entry.ValidateFn = desc.ValidateFn;
            m_entries.push_back(std::move(entry));
            m_index[desc.Key] = m_entries.size() - 1;
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

    void RuntimeComponentRegistry::ApplyComponents(const nlohmann::json& componentTables, Entity& entity) const
    {
        for (const auto& [key, node] : componentTables.items())
        {
            const Entry* entry = Find(key);
            if (!entry || !entry->ApplyFn)
                continue;

            if (!node.is_object())
                continue;

            entry->ApplyFn(node, entity);
        }
    }

    void RuntimeComponentRegistry::SerialiseComponents(const Entity& entity, nlohmann::json& componentTables) const
    {
        for (const Entry& entry : m_entries)
        {
            if (entry.SerialiseFn)
                entry.SerialiseFn(entity, componentTables);
        }
    }

    bool RuntimeComponentRegistry::ValidateComponent(std::string_view key, const nlohmann::json& componentData, std::string& error) const
    {
        const Entry* entry = Find(key);
        if (!entry || !entry->ValidateFn)
        {
            error = "component is not registered for scene authoring";
            return false;
        }

        return entry->ValidateFn(componentData, error);
    }

    bool RuntimeComponentRegistry::IsRegistered(std::string_view key) const
    {
        const Entry* entry = Find(key);
        return entry != nullptr && entry->ApplyFn != nullptr && entry->ValidateFn != nullptr;
    }

    const RuntimeComponentRegistry::Entry* RuntimeComponentRegistry::Find(std::string_view key) const
    {
        if (auto it = m_index.find(std::string(key)); it != m_index.end())
            return &m_entries[it->second];
        return nullptr;
    }
} // namespace Wayfinder
