#pragma once

#include "AppDescriptor.h"
#include "AppSubsystem.h"
#include "StateSubsystem.h"
#include "SubsystemManifest.h"
#include "SubsystemRegistry.h"
#include "core/Assert.h"
#include "core/InternedString.h"
#include "core/Log.h"
#include "core/Result.h"
#include "plugins/IPlugin.h"
#include "plugins/IRegistrar.h"
#include "plugins/LifecycleHooks.h"
#include "plugins/PluginConcepts.h"
#include "plugins/PluginDescriptor.h"

#include <concepts>
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Wayfinder
{
    class EngineContext;

    template<typename T>
    constexpr inline bool ALWAYS_FALSE = false;

    /**
     * @brief Central compositor for plugin-based engine configuration.
     *
     * Plugins register via AddPlugin<T>(), call Build() to declare
     * subsystems, states, lifecycle hooks, etc. through typed registrars.
     * Finalise() resolves dependencies, orders plugin builds, executes
     * them, and produces an immutable AppDescriptor.
     */
    class AppBuilder
    {
    public:
        AppBuilder() = default;
        ~AppBuilder() = default;

        AppBuilder(const AppBuilder&) = delete;
        auto operator=(const AppBuilder&) -> AppBuilder& = delete;
        AppBuilder(AppBuilder&&) noexcept = default;
        auto operator=(AppBuilder&&) noexcept -> AppBuilder& = default;

        /// Register a plugin (IPlugin-derived) or plugin group.
        template<typename T>
        void AddPlugin()
        {
            WAYFINDER_ASSERT(not m_finalised, "Cannot add plugins after Finalise()");

            if constexpr (PluginType<T>)
            {
                AddPluginImpl<T>();
            }
            else if constexpr (PluginGroupType<T>)
            {
                T group{};
                group.Build(*this);
            }
            else
            {
                static_assert(ALWAYS_FALSE<T>, "T must derive from IPlugin or satisfy PluginGroupType");
            }
        }

        /// Access a typed registrar. Lazily created on first access.
        template<typename TRegistrar>
            requires std::derived_from<TRegistrar, IRegistrar>
        auto GetRegistrar() -> TRegistrar&
        {
            const auto key = std::type_index(typeid(TRegistrar));
            auto it = m_registrars.find(key);
            if (it == m_registrars.end())
            {
                auto [inserted, ok] = m_registrars.emplace(key, std::make_unique<TRegistrar>());
                it = inserted;
            }
            return static_cast<TRegistrar&>(*it->second);
        }

        /// Extract a registrar from the store. Returns nullptr if not present.
        /// Used by Application to extract SubsystemRegistries for separate lifecycle.
        template<typename TRegistrar>
            requires std::derived_from<TRegistrar, IRegistrar>
        auto TakeRegistrar() -> std::unique_ptr<TRegistrar>
        {
            const auto key = std::type_index(typeid(TRegistrar));
            auto it = m_registrars.find(key);
            if (it == m_registrars.end())
            {
                return nullptr;
            }
            auto result = std::unique_ptr<TRegistrar>(static_cast<TRegistrar*>(it->second.release()));
            m_registrars.erase(it);
            return result;
        }

        /// Register an app-scoped subsystem.
        template<typename T>
            requires std::derived_from<T, AppSubsystem>
        void RegisterAppSubsystem(SubsystemDescriptor descriptor = {})
        {
            GetRegistrar<SubsystemRegistry<AppSubsystem>>().template Register<T>(std::move(descriptor));
        }

        /// Register a state-scoped subsystem.
        template<typename T>
            requires std::derived_from<T, StateSubsystem>
        void RegisterStateSubsystem(SubsystemDescriptor descriptor = {})
        {
            GetRegistrar<SubsystemRegistry<StateSubsystem>>().template Register<T>(std::move(descriptor));
        }

        /// Register an OnAppReady lifecycle hook.
        void OnAppReady(std::function<void(EngineContext&)> callback)
        {
            GetRegistrar<LifecycleHookRegistrar>().OnAppReady(std::move(callback));
        }

        /// Register an OnShutdown lifecycle hook.
        void OnShutdown(std::function<void()> callback)
        {
            GetRegistrar<LifecycleHookRegistrar>().OnShutdown(std::move(callback));
        }

        /// Register an OnStateEnter lifecycle hook for a specific state type.
        template<typename TState>
        void OnStateEnter(std::function<void(EngineContext&)> callback)
        {
            GetRegistrar<LifecycleHookRegistrar>().template OnStateEnter<TState>(std::move(callback));
        }

        /// Register an OnStateExit lifecycle hook for a specific state type.
        template<typename TState>
        void OnStateExit(std::function<void(EngineContext&)> callback)
        {
            GetRegistrar<LifecycleHookRegistrar>().template OnStateExit<TState>(std::move(callback));
        }

        /// Validate, order, build plugins, finalise registrars,
        /// and produce an immutable AppDescriptor.
        [[nodiscard]] auto Finalise() -> Result<AppDescriptor>;

    private:
        struct PluginEntry
        {
            std::type_index Type;
            InternedString Name;
            std::unique_ptr<IPlugin> Instance;
            std::vector<std::type_index> DependsOn;
        };

        template<typename T>
        void AddPluginImpl()
        {
            const auto type = std::type_index(typeid(T));
            if (m_pluginTypeToIndex.contains(type))
            {
                Log::Info(LogEngine, "Plugin already registered, skipping: {}", typeid(T).name());
                return;
            }

            auto plugin = std::make_unique<T>();
            auto descriptor = plugin->Describe();

            const size_t index = m_plugins.size();
            m_pluginTypeToIndex[type] = index;
            m_plugins.push_back(PluginEntry{
                .Type = type,
                .Name = descriptor.Name,
                .Instance = std::move(plugin),
                .DependsOn = std::move(descriptor.DependsOn),
            });
        }

        std::unordered_map<std::type_index, std::unique_ptr<IRegistrar>> m_registrars;
        std::vector<PluginEntry> m_plugins;
        std::unordered_map<std::type_index, size_t> m_pluginTypeToIndex;
        bool m_finalised = false;
    };

} // namespace Wayfinder
