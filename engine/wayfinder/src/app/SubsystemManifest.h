#pragma once

#include "core/Assert.h"
#include "core/Result.h"
#include "gameplay/Capability.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Wayfinder
{
    class EngineContext;
    template<typename TBase>
    class SubsystemRegistry;

    /// Variadic helper for building DependsOn type-index vectors.
    /// Usage: .DependsOn = Deps<Window, TimeSubsystem>()
    template<typename... TDeps>
    auto Deps() -> std::vector<std::type_index>
    {
        return {std::type_index(typeid(TDeps))...};
    }

    /// Metadata for subsystem registration. Uses designated initialisers.
    struct SubsystemDescriptor
    {
        CapabilitySet RequiredCapabilities;
        std::vector<std::type_index> DependsOn;
    };

    /**
     * @brief Runtime owner of subsystem instances, produced by SubsystemRegistry::Finalise().
     *
     * SubsystemManifest holds the processed output of a SubsystemRegistry:
     * the sorted entries, type maps, and init order. It owns the subsystem
     * instances and provides typed access via Get/TryGet. The registry
     * becomes build-time only after producing a manifest.
     *
     * Move-only. Constructed exclusively by SubsystemRegistry::Finalise().
     *
     * @tparam TBase The scope base class (e.g. AppSubsystem, StateSubsystem).
     */
    template<typename TBase>
    class SubsystemManifest
    {
    public:
        SubsystemManifest(const SubsystemManifest&) = delete;
        auto operator=(const SubsystemManifest&) -> SubsystemManifest& = delete;
        SubsystemManifest(SubsystemManifest&&) noexcept = default;
        auto operator=(SubsystemManifest&&) noexcept -> SubsystemManifest& = default;
        ~SubsystemManifest() = default;

        /// Create and initialise subsystems in topological order.
        /// Subsystems whose RequiredCapabilities are not satisfied by effectiveCaps are skipped.
        [[nodiscard]] auto Initialise(EngineContext& context, const CapabilitySet& effectiveCaps) -> Result<void>
        {
            for (const size_t i : m_initOrder)
            {
                auto& entry = m_entries[i];

                // Capability gating: skip if requirements not met (empty = always active)
                if (not entry.Descriptor.RequiredCapabilities.IsEmpty() and not effectiveCaps.HasAll(entry.Descriptor.RequiredCapabilities))
                {
                    continue;
                }

                // Dependency gating: skip if any declared dependency is not active.
                // This handles cascading: if A is gated off by capabilities,
                // B (which depends on A) is also skipped even if B's own
                // capabilities are satisfied.
                bool dependenciesMet = true;
                for (const auto& dep : entry.Descriptor.DependsOn)
                {
                    auto resolved = ResolveDependency(dep);
                    auto depIt = m_typeToIndex.find(resolved);
                    if (depIt == m_typeToIndex.end() or not m_entries[depIt->second].Active)
                    {
                        dependenciesMet = false;
                        break;
                    }
                }
                if (not dependenciesMet)
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
            WAYFINDER_ASSERT(ptr != nullptr, "SubsystemManifest::Get<{}> - not found or not active", typeid(T).name());
            return *ptr;
        }

        /// Retrieve a live subsystem by type (const). Asserts if not found or not active.
        template<typename T>
        auto Get() const -> const T&
        {
            const auto* ptr = TryGet<T>();
            WAYFINDER_ASSERT(ptr != nullptr, "SubsystemManifest::Get<{}> - not found or not active", typeid(T).name());
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

    private:
        friend class SubsystemRegistry<TBase>;

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

        /// Private constructor - only SubsystemRegistry::Finalise() creates manifests.
        SubsystemManifest(std::vector<Entry> entries, std::unordered_map<std::type_index, size_t> typeToIndex, std::unordered_map<std::type_index, std::type_index> abstractRedirect, std::vector<size_t> initOrder)
            : m_entries(std::move(entries)), m_typeToIndex(std::move(typeToIndex)), m_abstractRedirect(std::move(abstractRedirect)), m_initOrder(std::move(initOrder))
        {}

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
            for (size_t pos = 0; pos < m_initOrder.size(); ++pos)
            {
                if (m_initOrder[pos] == failedOrderIndex)
                {
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
                    auto& failedEntry = m_entries[failedOrderIndex];
                    failedEntry.Instance.reset();
                    failedEntry.Active = false;
                    return;
                }
            }
        }
    };

} // namespace Wayfinder
