#pragma once

#include "AppDescriptor.h"
#include "AppSubsystem.h"
#include "ConfigRegistrar.h"
#include "IApplicationState.h"
#include "IOverlay.h"
#include "OverlayManifest.h"
#include "StateManifest.h"
#include "StateSubsystem.h"
#include "SubsystemManifest.h"
#include "SubsystemRegistry.h"
#include "core/Assert.h"
#include "core/InternedString.h"
#include "core/Log.h"
#include "core/Result.h"
#include "plugins/IPlugin.h"
#include "plugins/IRegistrar.h"
#include "plugins/IStateUI.h"
#include "plugins/LifecycleHooks.h"
#include "plugins/PluginConcepts.h"
#include "plugins/PluginDescriptor.h"

#include <concepts>
#include <filesystem>
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
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

        // -- State / overlay / UI registration (Phase 4) ---------------------

        /// Nested builder proxy for per-state configuration (e.g. IStateUI).
        template<std::derived_from<IApplicationState> TState>
        class StateBuilder
        {
        public:
            explicit StateBuilder(AppBuilder& builder) : m_builder(builder) {}

            /// Register an IStateUI implementation for this state.
            template<std::derived_from<IStateUI> U>
            void SetUI()
            {
                m_builder.m_stateUIFactories[std::type_index(typeid(TState))] = []() -> std::unique_ptr<IStateUI>
                {
                    return std::make_unique<U>();
                };
            }

        private:
            AppBuilder& m_builder;
        };

        /// Register an application state with optional capabilities.
        template<std::derived_from<IApplicationState> T>
        void AddState(CapabilitySet capabilities = {})
        {
            WAYFINDER_ASSERT(not m_finalised, "Cannot add states after Finalise()");
            const auto type = std::type_index(typeid(T));
            WAYFINDER_ASSERT(not m_stateEntries.contains(type), "Duplicate state registration");
            m_stateEntries.insert_or_assign(type, StateManifest::StateEntry{
                                                      .Type = type,
                                                      .Factory = []() -> std::unique_ptr<IApplicationState>
            {
                return std::make_unique<T>();
            },
                                                      .Capabilities = std::move(capabilities),
                                                  });
        }

        /// Set the initial application state.
        template<std::derived_from<IApplicationState> T>
        void SetInitialState()
        {
            WAYFINDER_ASSERT(not m_finalised, "Cannot set initial state after Finalise()");
            m_initialState = std::type_index(typeid(T));
        }

        /// Declare a valid flat transition between two states.
        template<std::derived_from<IApplicationState> TFrom, std::derived_from<IApplicationState> TTo>
        void AddTransition()
        {
            WAYFINDER_ASSERT(not m_finalised, "Cannot add transitions after Finalise()");
            m_flatTransitions[std::type_index(typeid(TFrom))].insert(std::type_index(typeid(TTo)));
        }

        /// Declare a state as pushable (any state can push to it).
        template<std::derived_from<IApplicationState> T>
        void AllowPush()
        {
            WAYFINDER_ASSERT(not m_finalised, "Cannot modify push states after Finalise()");
            m_pushableStates.insert(std::type_index(typeid(T)));
        }

        /// Register an overlay with an optional descriptor.
        template<std::derived_from<IOverlay> T>
        void RegisterOverlay(OverlayDescriptor descriptor = {})
        {
            WAYFINDER_ASSERT(not m_finalised, "Cannot register overlays after Finalise()");
            const auto type = std::type_index(typeid(T));
            m_overlayEntries.push_back(OverlayManifest::OverlayEntry{
                .Type = type,
                .Factory = []() -> std::unique_ptr<IOverlay>
            {
                return std::make_unique<T>();
            },
                .Descriptor = std::move(descriptor),
            });
        }

        /// Get a per-state builder proxy for registering IStateUI.
        template<std::derived_from<IApplicationState> T>
        auto ForState() -> StateBuilder<T>
        {
            WAYFINDER_ASSERT(not m_finalised, "Cannot modify state config after Finalise()");
            return StateBuilder<T>{*this};
        }

        /// Set project paths for config file resolution.
        /// Called by Application before the plugin Build() phase.
        void SetProjectPaths(const std::filesystem::path& configDir, const std::filesystem::path& savedDir);

        /// Load a per-plugin config struct from TOML with 3-tier layering.
        /// Struct defaults < config/<key>.toml < saved/config/<key>.toml.
        /// Cached: multiple calls with the same key parse the file once.
        /// If no project paths are set or files are missing, returns T{}.
        template<typename T>
        auto LoadConfig(std::string_view key) -> T
        {
            auto& configReg = GetRegistrar<ConfigRegistrar>();
            configReg.DeclareConfig(key, std::type_index(typeid(T)));

            if (m_configDir.empty())
            {
                return T{};
            }

            auto tableResult = configReg.LoadTable(key, m_configDir, m_savedDir);
            if (not tableResult)
            {
                Log::Warn(LogEngine, "Config load failed for '{}': {}", key, tableResult.error().GetMessage());
                return T{};
            }

            return T::FromToml(**tableResult);
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
        std::filesystem::path m_configDir;
        std::filesystem::path m_savedDir;

        // Phase 4: state / overlay / UI registration
        std::unordered_map<std::type_index, StateManifest::StateEntry> m_stateEntries;
        std::type_index m_initialState{typeid(void)};
        std::unordered_map<std::type_index, std::unordered_set<std::type_index>> m_flatTransitions;
        std::unordered_set<std::type_index> m_pushableStates;
        std::vector<OverlayManifest::OverlayEntry> m_overlayEntries;
        std::unordered_map<std::type_index, std::function<std::unique_ptr<IStateUI>()>> m_stateUIFactories;

        bool m_finalised = false;
    };

} // namespace Wayfinder
