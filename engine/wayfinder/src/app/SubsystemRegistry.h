#pragma once

#include "app/SubsystemManifest.h"
#include "core/Assert.h"
#include "core/Result.h"
#include "core/TopologicalSort.h"
#include "gameplay/Capability.h"
#include "gameplay/Tag.h"
#include "wayfinder_exports.h"

#include <concepts>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Wayfinder
{

    /**
     * @brief Build-time dependency-ordered subsystem registry.
     *
     * Subsystems are registered with descriptors specifying dependencies
     * and required capabilities. Finalise() validates the dependency graph
     * (detecting cycles and missing dependencies) and produces an immutable
     * SubsystemManifest that owns runtime instances.
     *
     * After Finalise(), the registry is consumed. All runtime access
     * (Initialise, Shutdown, Get, TryGet) lives on SubsystemManifest.
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
            m_entries.push_back(typename SubsystemManifest<TBase>::Entry{
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
            m_entries.push_back(typename SubsystemManifest<TBase>::Entry{
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

        /// Validate the dependency graph, produce a SubsystemManifest, and freeze registration.
        /// Detects cycles, missing dependencies, and invalid abstract redirects.
        /// After success, the registry's internal data has been moved into the manifest.
        [[nodiscard]] auto Finalise() -> Result<SubsystemManifest<TBase>>
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

            for (size_t i = 0; i < nodeCount; ++i)
            {
                for (const auto& dep : m_entries[i].Descriptor.DependsOn)
                {
                    auto resolved = ResolveDependency(dep);
                    const size_t depIdx = m_typeToIndex.at(resolved);
                    adjacency[depIdx].push_back(i);
                }
            }

            // Use shared TopologicalSort utility
            auto sortResult = Wayfinder::TopologicalSort(nodeCount, adjacency, [this](size_t idx) -> std::string
            {
                return m_entries[idx].DebugName;
            });

            if (sortResult.HasCycle)
            {
                return MakeError(std::format("Cycle detected: {}", sortResult.CyclePath));
            }

            m_finalised = true;

            // Construct manifest by moving all internal state into it
            return SubsystemManifest<TBase>{
                std::move(m_entries),
                std::move(m_typeToIndex),
                std::move(m_abstractRedirect),
                std::move(sortResult.Order),
            };
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
        std::vector<typename SubsystemManifest<TBase>::Entry> m_entries;
        std::unordered_map<std::type_index, size_t> m_typeToIndex;
        std::unordered_map<std::type_index, std::type_index> m_abstractRedirect;
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
    };

} // namespace Wayfinder
