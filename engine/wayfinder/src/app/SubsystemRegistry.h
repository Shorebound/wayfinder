#pragma once

#include "app/SubsystemManifest.h"
#include "core/Assert.h"
#include "core/Result.h"
#include "core/TopologicalSort.h"
#include "gameplay/Capability.h"
#include "gameplay/Tag.h"
#include "wayfinder_exports.h"

#include <algorithm>
#include <concepts>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Wayfinder
{
    class EngineContext;

    /**
     * @brief Dependency-ordered, capability-gated subsystem registry.
     *
     * Replaces SubsystemCollection<TBase> for the v2 architecture.
     * Subsystems are registered with descriptors specifying dependencies
     * and required capabilities. Finalise() validates the dependency graph
     * (detecting cycles and missing dependencies). Initialise() creates
     * instances in topological order, skipping those whose capability
     * requirements are not met.
     *
     * @tparam TBase The scope base class (e.g. AppSubsystem, StateSubsystem).
     */
    template<typename TBase>
    class SubsystemRegistry
    {
    public:
        /// Register a concrete subsystem type with an optional descriptor.
        template<typename T>
            requires std::derived_from<T, TBase>
        void Register(SubsystemDescriptor descriptor = {})
        {
            WAYFINDER_ASSERT(not m_finalised, "Cannot register after Finalise()");

            const std::type_index type{typeid(T)};
            WAYFINDER_ASSERT(not m_typeToIndex.contains(type), "Duplicate subsystem registration: {}", typeid(T).name());

            const size_t index = m_entries.size();
            m_entries.push_back(Entry{
                type,
                std::nullopt,
                std::move(descriptor),
                []() -> std::unique_ptr<TBase>
            {
                return std::make_unique<T>();
            },
                nullptr,
                typeid(T).name(),
                false,
            });
            m_typeToIndex[type] = index;
        }

        /// Register a concrete subsystem type with an abstract type alias.
        /// The subsystem is queryable by both concrete and abstract type.
        template<typename TConcrete, typename TAbstract>
            requires(std::derived_from<TConcrete, TBase> and std::derived_from<TConcrete, TAbstract>)
        void Register(SubsystemDescriptor descriptor = {})
        {
            WAYFINDER_ASSERT(not m_finalised, "Cannot register after Finalise()");

            const std::type_index concreteType{typeid(TConcrete)};
            const std::type_index abstractType{typeid(TAbstract)};

            WAYFINDER_ASSERT(not m_typeToIndex.contains(concreteType), "Duplicate subsystem registration: {}", typeid(TConcrete).name());
            WAYFINDER_ASSERT(not m_abstractRedirect.contains(abstractType), "Duplicate abstract registration for: {}", typeid(TAbstract).name());

            const size_t index = m_entries.size();
            m_entries.push_back(Entry{
                concreteType,
                abstractType,
                std::move(descriptor),
                []() -> std::unique_ptr<TBase>
            {
                return std::make_unique<TConcrete>();
            },
                nullptr,
                typeid(TConcrete).name(),
                false,
            });
            m_typeToIndex[concreteType] = index;
            m_abstractRedirect.insert_or_assign(abstractType, concreteType);
        }

        /// Validate the dependency graph and freeze registration.
        /// Detects cycles, missing dependencies, and invalid abstract redirects.
        [[nodiscard]] auto Finalise() -> Result<void>
        {
            // Validate all DependsOn targets exist
            for (const auto& entry : m_entries)
            {
                for (const auto& dep : entry.Descriptor.DependsOn)
                {
                    auto resolved = ResolveDependency(dep);
                    if (not m_typeToIndex.contains(resolved))
                    {
                        return MakeError(std::format("Subsystem '{}' declares dependency on unregistered type", entry.DebugName));
                    }
                }
            }

            // Build adjacency list (dep -> dependents)
            const size_t nodeCount = m_entries.size();
            std::vector<std::vector<size_t>> adjacency(nodeCount);
            std::vector<size_t> inDegree(nodeCount, 0);

            for (size_t i = 0; i < nodeCount; ++i)
            {
                for (const auto& dep : m_entries[i].Descriptor.DependsOn)
                {
                    auto resolved = ResolveDependency(dep);
                    const size_t depIdx = m_typeToIndex.at(resolved);
                    adjacency[depIdx].push_back(i);
                    ++inDegree[i];
                }
            }

            // Kahn's algorithm for topological sort
            std::queue<size_t> ready;
            for (size_t i = 0; i < nodeCount; ++i)
            {
                if (inDegree[i] == 0)
                {
                    ready.push(i);
                }
            }

            m_initOrder.clear();
            m_initOrder.reserve(nodeCount);

            while (not ready.empty())
            {
                const size_t current = ready.front();
                ready.pop();
                m_initOrder.push_back(current);

                for (const size_t dependent : adjacency[current])
                {
                    --inDegree[dependent];
                    if (inDegree[dependent] == 0)
                    {
                        ready.push(dependent);
                    }
                }
            }

            if (m_initOrder.size() != nodeCount)
            {
                // Extract cycle path from residual nodes
                auto cyclePath = ExtractCyclePath(inDegree, adjacency);
                return MakeError(std::format("Cycle detected: {}", cyclePath));
            }

            m_finalised = true;
            return {};
        }

        /// Create and initialise subsystems in topological order.
        /// Subsystems whose RequiredCapabilities are not satisfied by effectiveCaps are skipped.
        [[nodiscard]] auto Initialise(EngineContext& context, const CapabilitySet& effectiveCaps) -> Result<void>
        {
            WAYFINDER_ASSERT(m_finalised, "Must call Finalise() before Initialise()");

            for (const size_t i : m_initOrder)
            {
                auto& entry = m_entries[i];

                // Capability gating: skip if requirements not met (empty = always active)
                if (not entry.Descriptor.RequiredCapabilities.IsEmpty() and not effectiveCaps.HasAll(entry.Descriptor.RequiredCapabilities))
                {
                    continue;
                }

                entry.Instance = entry.Factory();
                auto result = entry.Instance->Initialise(context);

                if (not result)
                {
                    // Reverse-shutdown already-initialised subsystems
                    ReverseShutdownFrom(i);
                    return std::unexpected(result.error());
                }

                entry.Active = true;
            }

            return {};
        }

        /// Shutdown all active subsystems in reverse topological order.
        void Shutdown()
        {
            for (auto it = m_initOrder.rbegin(); it != m_initOrder.rend(); ++it)
            {
                auto& entry = m_entries[*it];
                if (entry.Active and entry.Instance)
                {
                    entry.Instance->Shutdown();
                    entry.Active = false;
                    entry.Instance.reset();
                }
            }
        }

        /// Retrieve a live subsystem by type. Asserts if not found or not active.
        template<typename T>
        auto Get() -> T&
        {
            auto* ptr = TryGet<T>();
            WAYFINDER_ASSERT(ptr != nullptr, "SubsystemRegistry::Get<{}> - not found or not active", typeid(T).name());
            return *ptr;
        }

        /// Retrieve a live subsystem by type (const). Asserts if not found or not active.
        template<typename T>
        auto Get() const -> const T&
        {
            const auto* ptr = TryGet<T>();
            WAYFINDER_ASSERT(ptr != nullptr, "SubsystemRegistry::Get<{}> - not found or not active", typeid(T).name());
            return *ptr;
        }

        /// Retrieve a live subsystem by type, or nullptr if not found/not active.
        template<typename T>
        auto TryGet() -> T*
        {
            const auto typeIdx = ResolveType(std::type_index(typeid(T)));
            if (not typeIdx.has_value())
            {
                return nullptr;
            }

            auto it = m_typeToIndex.find(*typeIdx);
            if (it == m_typeToIndex.end())
            {
                return nullptr;
            }

            auto& entry = m_entries[it->second];
            if (not entry.Active or not entry.Instance)
            {
                return nullptr;
            }

            return static_cast<T*>(entry.Instance.get());
        }

        /// Retrieve a live subsystem by type (const), or nullptr if not found/not active.
        template<typename T>
        auto TryGet() const -> const T*
        {
            const auto typeIdx = ResolveType(std::type_index(typeid(T)));
            if (not typeIdx.has_value())
            {
                return nullptr;
            }

            auto it = m_typeToIndex.find(*typeIdx);
            if (it == m_typeToIndex.end())
            {
                return nullptr;
            }

            const auto& entry = m_entries[it->second];
            if (not entry.Active or not entry.Instance)
            {
                return nullptr;
            }

            return static_cast<const T*>(entry.Instance.get());
        }

        /// Check if a type is registered (concrete or abstract).
        template<typename T>
        [[nodiscard]] auto IsRegistered() const -> bool
        {
            const auto typeIdx = ResolveType(std::type_index(typeid(T)));
            if (not typeIdx.has_value())
            {
                return false;
            }
            return m_typeToIndex.contains(*typeIdx);
        }

        /// Check if the registry has been finalised.
        [[nodiscard]] auto IsFinalised() const -> bool
        {
            return m_finalised;
        }

    private:
        struct Entry
        {
            std::type_index ConcreteType;
            std::optional<std::type_index> AbstractType;
            SubsystemDescriptor Descriptor;
            std::function<std::unique_ptr<TBase>()> Factory;
            std::unique_ptr<TBase> Instance;
            std::string DebugName;
            bool Active = false;

            Entry(std::type_index concreteType, std::optional<std::type_index> abstractType, SubsystemDescriptor descriptor, std::function<std::unique_ptr<TBase>()> factory, std::unique_ptr<TBase> instance,
                std::string debugName, bool active)
                : ConcreteType(concreteType), AbstractType(std::move(abstractType)), Descriptor(std::move(descriptor)), Factory(std::move(factory)), Instance(std::move(instance)), DebugName(std::move(debugName)),
                  Active(active)
            {}
        };

        std::vector<Entry> m_entries;
        std::unordered_map<std::type_index, size_t> m_typeToIndex;
        std::unordered_map<std::type_index, std::type_index> m_abstractRedirect;
        std::vector<size_t> m_initOrder;
        bool m_finalised = false;

        /// Resolve a type_index through abstract redirect if needed.
        [[nodiscard]] auto ResolveType(std::type_index type) const -> std::optional<std::type_index>
        {
            if (m_typeToIndex.contains(type))
            {
                return type;
            }

            auto it = m_abstractRedirect.find(type);
            if (it != m_abstractRedirect.end())
            {
                return it->second;
            }

            return std::nullopt;
        }

        /// Resolve a dependency type_index - dependencies may reference abstract types.
        [[nodiscard]] auto ResolveDependency(std::type_index dep) const -> std::type_index
        {
            auto it = m_abstractRedirect.find(dep);
            if (it != m_abstractRedirect.end())
            {
                return it->second;
            }
            return dep;
        }

        /// Reverse-shutdown already-initialised subsystems when Initialise fails.
        void ReverseShutdownFrom(size_t failedOrderIndex)
        {
            // Find the position of the failed index in m_initOrder
            for (size_t pos = 0; pos < m_initOrder.size(); ++pos)
            {
                if (m_initOrder[pos] == failedOrderIndex)
                {
                    // Shut down everything before this position, in reverse
                    for (size_t rev = pos; rev-- > 0;)
                    {
                        auto& entry = m_entries[m_initOrder[rev]];
                        if (entry.Active and entry.Instance)
                        {
                            entry.Instance->Shutdown();
                            entry.Active = false;
                            entry.Instance.reset();
                        }
                    }
                    // Also reset the failed instance
                    auto& failedEntry = m_entries[failedOrderIndex];
                    failedEntry.Instance.reset();
                    failedEntry.Active = false;
                    return;
                }
            }
        }

        /// Extract a human-readable cycle path from residual in-degree nodes.
        [[nodiscard]] auto ExtractCyclePath(const std::vector<size_t>& inDegree, const std::vector<std::vector<size_t>>& adjacency) const -> std::string
        {
            // Find a starting node with non-zero in-degree
            size_t start = 0;
            for (size_t i = 0; i < inDegree.size(); ++i)
            {
                if (inDegree[i] > 0)
                {
                    start = i;
                    break;
                }
            }

            // Follow edges from nodes with non-zero in-degree to trace the cycle
            std::vector<bool> visited(m_entries.size(), false);
            std::vector<size_t> path;
            size_t current = start;

            while (not visited[current])
            {
                visited[current] = true;
                path.push_back(current);

                // Find a neighbour that also has non-zero in-degree (part of cycle)
                bool found = false;
                for (const size_t next : adjacency[current])
                {
                    if (inDegree[next] > 0)
                    {
                        current = next;
                        found = true;
                        break;
                    }
                }
                if (not found)
                {
                    break;
                }
            }

            // current is the node we revisited - extract the cycle from the path
            std::string result;
            bool inCycle = false;
            for (const size_t node : path)
            {
                if (node == current)
                {
                    inCycle = true;
                }
                if (inCycle)
                {
                    if (not result.empty())
                    {
                        result += " -> ";
                    }
                    result += m_entries[node].DebugName;
                }
            }
            // Close the cycle
            if (not result.empty())
            {
                result += " -> ";
                result += m_entries[current].DebugName;
            }

            return result;
        }
    };

} // namespace Wayfinder
