#include "SystemRegistrar.h"
#include "core/Log.h"

#include <algorithm>
#include <queue>
#include <unordered_map>

#include "ecs/Flecs.h"

namespace Wayfinder::Plugins
{
    void SystemRegistrar::Register(std::string name, std::function<void(flecs::world&)> factory, Wayfinder::RunCondition condition, std::vector<std::string> after, std::vector<std::string> before)
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

        WAYFINDER_INFO(LogEngine, "SystemRegistrar: registered system '{}'{}", name, condition ? " (conditioned)" : "");
        m_descriptors.push_back({.Name = std::move(name), .Factory = std::move(factory), .Condition = std::move(condition), .After = std::move(after), .Before = std::move(before)});
    }

    void SystemRegistrar::ApplyToWorld(flecs::world& world) const
    {
        // Topological sort of systems based on After/Before declarations.
        std::unordered_map<std::string, size_t> nameToIdx;
        nameToIdx.reserve(m_descriptors.size());

        for (size_t descriptorIndex = 0; descriptorIndex < m_descriptors.size(); ++descriptorIndex)
        {
            const Descriptor& descriptor = m_descriptors.at(descriptorIndex);
            nameToIdx.emplace(descriptor.Name, descriptorIndex);
        }

        const size_t n = m_descriptors.size();
        std::vector<std::vector<size_t>> adj(n);
        std::vector<size_t> inDegree(n, 0);

        for (size_t descriptorIndex = 0; descriptorIndex < n; ++descriptorIndex)
        {
            const Descriptor& descriptor = m_descriptors.at(descriptorIndex);

            // "i runs after j" means edge j -> i
            for (const auto& dep : descriptor.After)
            {
                if (auto it = nameToIdx.find(dep); it != nameToIdx.end())
                {
                    adj.at(it->second).push_back(descriptorIndex);
                    ++inDegree.at(descriptorIndex);
                }
                else
                {
                    WAYFINDER_WARN(LogEngine,
                        "SystemRegistrar: system '{}' declares After '{}' but no such "
                        "system is registered; ignoring ordering constraint.",
                        descriptor.Name, dep);
                }
            }

            // "i runs before j" means edge i -> j
            for (const auto& dep : descriptor.Before)
            {
                if (auto it = nameToIdx.find(dep); it != nameToIdx.end())
                {
                    adj.at(descriptorIndex).push_back(it->second);
                    ++inDegree.at(it->second);
                }
                else
                {
                    WAYFINDER_WARN(LogEngine,
                        "SystemRegistrar: system '{}' declares Before '{}' but no such "
                        "system is registered; ignoring ordering constraint.",
                        descriptor.Name, dep);
                }
            }
        }

        // Kahn's algorithm
        std::queue<size_t> ready;
        for (size_t descriptorIndex = 0; descriptorIndex < n; ++descriptorIndex)
        {
            if (inDegree.at(descriptorIndex) == 0)
            {
                ready.push(descriptorIndex);
            }
        }

        std::vector<size_t> sorted;
        sorted.reserve(n);
        while (!ready.empty())
        {
            const size_t cur = ready.front();
            ready.pop();
            sorted.push_back(cur);

            for (const size_t next : adj.at(cur))
            {
                if (--inDegree.at(next) == 0)
                {
                    ready.push(next);
                }
            }
        }

        if (sorted.size() != n)
        {
            // Identify systems that are part of the cycle (non-zero in-degree).
            std::string cycleMembers;
            for (size_t descriptorIndex = 0; descriptorIndex < n; ++descriptorIndex)
            {
                if (inDegree.at(descriptorIndex) > 0)
                {
                    if (!cycleMembers.empty())
                    {
                        cycleMembers += ", ";
                    }
                    cycleMembers += m_descriptors.at(descriptorIndex).Name;
                }
            }

            WAYFINDER_ERROR(LogEngine,
                "SystemRegistrar: cycle detected in system ordering constraints! "
                "Cyclic systems skipped: {}. Initialising {} non-cyclic system(s) in topological order.",
                cycleMembers, sorted.size());
        }

        for (const size_t idx : sorted)
        {
            m_descriptors.at(idx).Factory(world);
        }
    }

} // namespace Wayfinder::Plugins
