#include "ModuleRegistry.h"
#include "Log.h"

#include <algorithm>
#include <queue>
#include <unordered_map>

#include <flecs.h>

namespace Wayfinder
{
    ModuleRegistry::ModuleRegistry(const ProjectDescriptor& project,
                                   const EngineConfig& config)
        : m_project(project), m_config(config) {}

    void ModuleRegistry::RegisterSystem(std::string name, SystemFactory factory,
                                         RunCondition condition,
                                         std::vector<std::string> after,
                                         std::vector<std::string> before)
    {
        for (const auto& existing : m_systems)
        {
            if (existing.Name == name)
            {
                WAYFINDER_ERROR(LogEngine, "ModuleRegistry: duplicate system name '{}' — registration rejected", name);
                return;
            }
        }

        WAYFINDER_INFO(LogEngine, "ModuleRegistry: registered system '{}'{}", name,
                       condition ? " (conditioned)" : "");
        m_systems.push_back({std::move(name), std::move(factory), std::move(condition),
                             std::move(after), std::move(before)});
    }

    void ModuleRegistry::RegisterComponent(ComponentDescriptor descriptor)
    {
        WAYFINDER_INFO(LogEngine, "ModuleRegistry: registered component '{}'", descriptor.Key);
        m_components.push_back(std::move(descriptor));
    }

    void ModuleRegistry::RegisterGlobal(std::string name, GlobalFactory factory)
    {
        WAYFINDER_INFO(LogEngine, "ModuleRegistry: registered global '{}'", name);
        m_globals.push_back({std::move(name), std::move(factory)});
    }

    void ModuleRegistry::RegisterState(StateDescriptor descriptor)
    {
        WAYFINDER_INFO(LogEngine, "ModuleRegistry: registered state '{}'", descriptor.Name);
        m_states.push_back(std::move(descriptor));
    }

    void ModuleRegistry::SetInitialState(std::string stateName)
    {
        m_initialState = std::move(stateName);
    }

    GameplayTag ModuleRegistry::RegisterTag(std::string tagName, std::string comment)
    {
        WAYFINDER_INFO(LogEngine, "ModuleRegistry: registered tag '{}'", tagName);
        GameplayTag tag{tagName};
        m_tags.push_back({std::move(tagName), std::move(comment)});
        return tag;
    }

    void ModuleRegistry::RegisterTagFile(std::string relativePath)
    {
        WAYFINDER_INFO(LogEngine, "ModuleRegistry: registered tag file '{}'", relativePath);
        m_tagFiles.push_back(std::move(relativePath));
    }

    void ModuleRegistry::ApplyToWorld(flecs::world& world) const
    {
        // Component registration is handled by RuntimeComponentRegistry,
        // which merges core + game entries. Here we only apply globals and systems.
        for (const auto& desc : m_globals)
        {
            desc.Factory(world);
        }

        // Topological sort of systems based on After/Before declarations.
        // Build name -> index map.
        std::unordered_map<std::string, size_t> nameToIdx;
        for (size_t i = 0; i < m_systems.size(); ++i)
            nameToIdx[m_systems[i].Name] = i;

        const size_t n = m_systems.size();
        std::vector<std::vector<size_t>> adj(n);    // adj[i] = systems that must come after i
        std::vector<size_t> inDegree(n, 0);

        for (size_t i = 0; i < n; ++i)
        {
            // "i runs after j" means edge j -> i
            for (const auto& dep : m_systems[i].After)
            {
                if (auto it = nameToIdx.find(dep); it != nameToIdx.end())
                {
                    adj[it->second].push_back(i);
                    ++inDegree[i];
                }
                else
                {
                    WAYFINDER_WARNING(
                        LogEngine,
                        "ModuleRegistry: system '{}' declares After '{}' but no such "
                        "system is registered; ignoring ordering constraint.",
                        m_systems[i].Name,
                        dep);
                }
            }

            // "i runs before j" means edge i -> j
            for (const auto& dep : m_systems[i].Before)
            {
                if (auto it = nameToIdx.find(dep); it != nameToIdx.end())
                {
                    adj[i].push_back(it->second);
                    ++inDegree[it->second];
                }
                else
                {
                    WAYFINDER_WARNING(
                        LogEngine,
                        "ModuleRegistry: system '{}' declares Before '{}' but no such "
                        "system is registered; ignoring ordering constraint.",
                        m_systems[i].Name,
                        dep);
                }
            }
        }

        // Kahn's algorithm
        std::queue<size_t> ready;
        for (size_t i = 0; i < n; ++i)
        {
            if (inDegree[i] == 0)
                ready.push(i);
        }

        std::vector<size_t> sorted;
        sorted.reserve(n);
        while (!ready.empty())
        {
            const size_t cur = ready.front();
            ready.pop();
            sorted.push_back(cur);

            for (const size_t next : adj[cur])
            {
                if (--inDegree[next] == 0)
                    ready.push(next);
            }
        }

        if (sorted.size() != n)
        {
            // Identify systems that are part of the cycle (non-zero in-degree).
            std::string cycleMembers;
            for (size_t i = 0; i < n; ++i)
            {
                if (inDegree[i] > 0)
                {
                    if (!cycleMembers.empty())
                        cycleMembers += ", ";
                    cycleMembers += m_systems[i].Name;
                }
            }

            WAYFINDER_ERROR(LogEngine,
                "ModuleRegistry: cycle detected in system ordering constraints! "
                "Systems involved: {}. Falling back to registration order.",
                cycleMembers);
            for (const auto& desc : m_systems)
                desc.Factory(world);
        }
        else
        {
            for (const size_t idx : sorted)
                m_systems[idx].Factory(world);
        }
    }

    const ProjectDescriptor& ModuleRegistry::GetProject() const { return m_project; }

    const EngineConfig& ModuleRegistry::GetConfig() const { return m_config; }

} // namespace Wayfinder
