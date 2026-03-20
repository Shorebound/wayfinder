#include "SystemRegistrar.h"
#include "../Log.h"

#include <algorithm>
#include <queue>
#include <unordered_map>

#include <flecs.h>

namespace Wayfinder
{
    void SystemRegistrar::Register(std::string name,
                                   std::function<void(flecs::world&)> factory,
                                   RunCondition condition,
                                   std::vector<std::string> after,
                                   std::vector<std::string> before)
    {
        if (!factory)
        {
            WAYFINDER_ERROR(LogEngine, "SystemRegistrar: empty factory for system '{}' — registration rejected", name);
            return;
        }

        for (const auto& existing : m_descriptors)
        {
            if (existing.Name == name)
            {
                WAYFINDER_ERROR(LogEngine, "SystemRegistrar: duplicate system name '{}' — registration rejected", name);
                return;
            }
        }

        WAYFINDER_INFO(LogEngine, "SystemRegistrar: registered system '{}'{}", name,
                       condition ? " (conditioned)" : "");
        m_descriptors.push_back({std::move(name), std::move(factory), std::move(condition),
                                 std::move(after), std::move(before)});
    }

    void SystemRegistrar::ApplyToWorld(flecs::world& world) const
    {
        // Topological sort of systems based on After/Before declarations.
        std::unordered_map<std::string, size_t> nameToIdx;
        for (size_t i = 0; i < m_descriptors.size(); ++i)
            nameToIdx[m_descriptors[i].Name] = i;

        const size_t n = m_descriptors.size();
        std::vector<std::vector<size_t>> adj(n);
        std::vector<size_t> inDegree(n, 0);

        for (size_t i = 0; i < n; ++i)
        {
            // "i runs after j" means edge j -> i
            for (const auto& dep : m_descriptors[i].After)
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
                        "SystemRegistrar: system '{}' declares After '{}' but no such "
                        "system is registered; ignoring ordering constraint.",
                        m_descriptors[i].Name,
                        dep);
                }
            }

            // "i runs before j" means edge i -> j
            for (const auto& dep : m_descriptors[i].Before)
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
                        "SystemRegistrar: system '{}' declares Before '{}' but no such "
                        "system is registered; ignoring ordering constraint.",
                        m_descriptors[i].Name,
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
                    cycleMembers += m_descriptors[i].Name;
                }
            }

            WAYFINDER_ERROR(LogEngine,
                "SystemRegistrar: cycle detected in system ordering constraints! "
                "Cyclic systems skipped: {}. Initialising {} non-cyclic system(s) in topological order.",
                cycleMembers,
                sorted.size());
        }

        for (const size_t idx : sorted)
            m_descriptors[idx].Factory(world);
    }

} // namespace Wayfinder
