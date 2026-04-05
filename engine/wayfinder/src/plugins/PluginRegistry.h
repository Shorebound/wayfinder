#pragma once

#include "Plugin.h"
#include "app/Subsystem.h"
#include "gameplay/GameState.h"
#include "plugins/registrars/StateRegistrar.h"
#include "plugins/registrars/SystemRegistrar.h"
#include "plugins/registrars/TagRegistrar.h"
#include "wayfinder_exports.h"

#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <typeindex>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace flecs
{
    struct world;
}

namespace Wayfinder
{
    class Entity;
    struct EngineConfig;
    struct ProjectDescriptor;
}

namespace Wayfinder::Plugins
{
    /**
     * @brief Collects ECS registrations from plugins during Plugin::Build().
     *
     * This is a descriptor store, not a live world facade. Plugins declare
     * their systems and components here, and the engine applies those
     * declarations once into the persistent flecs::world at startup.
     *
     * Internally delegates system, state, and tag storage to focused
     * sub-registries (SystemRegistrar, StateRegistrar, TagRegistrar).
     */
    class WAYFINDER_API PluginRegistry
    {
    public:
        using SystemFactory = std::function<void(flecs::world&)>;
        using ComponentRegisterFn = void (*)(flecs::world& world);
        using ComponentApplyFn = void (*)(const nlohmann::json& componentData, ::Wayfinder::Entity& entity);
        using ComponentSerialiseFn = void (*)(const ::Wayfinder::Entity& entity, nlohmann::json& componentTables);
        using ComponentValidateFn = bool (*)(const nlohmann::json& componentData, std::string& error);
        using GlobalFactory = std::function<void(flecs::world&)>;

        /// Type aliases that keep external consumers working unchanged.
        using SystemDescriptor = SystemRegistrar::Descriptor;
        using StateDescriptor = StateRegistrar::Descriptor;
        using TagDescriptor = TagRegistrar::Descriptor;

        /**
         * @brief Describes an ECS component registration for the runtime world.
         *
         * For scene authoring, set Apply/Serialise/Validate. For runtime-only types
         * (e.g. cached world transforms), only \ref Key and \ref RegisterFn are required.
         */
        struct ComponentDescriptor
        {
            std::string Key;
            ComponentRegisterFn RegisterFn = nullptr;
            ComponentApplyFn ApplyFn = nullptr;
            ComponentSerialiseFn SerialiseFn = nullptr;
            ComponentValidateFn ValidateFn = nullptr;
        };

        /**
         * @brief Describes a typed world singleton (global data).
         */
        struct GlobalDescriptor
        {
            std::string Name;
            GlobalFactory Factory;
        };

        PluginRegistry(const ::Wayfinder::ProjectDescriptor& project, const ::Wayfinder::EngineConfig& config);

        /// Add a plugin. The plugin's Build() is called immediately.
        template<typename T>
        void AddPlugin()
        {
            AddPlugin(std::make_unique<T>());
        }

        /// Add an externally-created plugin instance.
        void AddPlugin(std::unique_ptr<Plugin> plugin);

        /// Call OnStartup() on all plugins in registration order.
        void NotifyStartup();

        /// Call OnShutdown() on all plugins in reverse registration order.
        void NotifyShutdown();

        /// Call every \ref ComponentDescriptor::RegisterFn from \ref RegisterComponent entries.
        /// Used when applying plugins without a \ref RuntimeComponentRegistry (e.g. headless tests).
        void ApplyComponentRegisterFns(flecs::world& world) const;

        /// Register a named ECS system factory.  The factory will be called
        /// once when the engine creates its persistent flecs::world.
        /// An optional RunCondition controls whether the system is active.
        /// Optional After/Before lists declare ordering relative to other systems.
        void RegisterSystem(std::string name, SystemFactory factory, ::Wayfinder::RunCondition condition = {}, std::vector<std::string> after = {}, std::vector<std::string> before = {});

        /// Register a serialisable component for scene authoring.
        void RegisterComponent(ComponentDescriptor descriptor);

        /// Register a typed world singleton (global data). An empty \p factory is rejected (logged, no-op), matching \ref RegisterSystem.
        void RegisterGlobal(std::string name, GlobalFactory factory);

        /// Register a named game state with optional enter/exit callbacks.
        void RegisterState(StateDescriptor descriptor);

        /// Set the initial game state. Game will transition to this state
        /// during initialisation, after all registrations are applied.
        void SetInitialState(std::string stateName);

        /// Register a gameplay tag name. Returns a Tag that can be
        /// captured and used immediately (e.g. passed to HasTag run conditions).
        ::Wayfinder::Tag RegisterTag(std::string_view tagName, std::string_view comment = {});

        /// Register a tag definition file to be loaded at startup.
        /// Path is relative to the project's config directory.
        void RegisterTagFile(std::string relativePath);

        /// Register a game subsystem type. It will be created automatically
        /// during Game initialisation alongside engine-core subsystems.
        /// An optional static predicate is checked before construction.
        template<typename T>
            requires std::derived_from<T, ::Wayfinder::GameSubsystem>
        void RegisterSubsystem(::Wayfinder::SubsystemCollection<::Wayfinder::GameSubsystem>::PredicateFn predicate = nullptr)
        {
            m_subsystemFactories.push_back({std::type_index(typeid(T)), []() -> std::unique_ptr<::Wayfinder::GameSubsystem>
            {
                return std::make_unique<T>();
            }, predicate});
        }

        /// Apply all registered system factories into the given world.
        /// Called once by Game::InitialiseWorld after core ECS setup.
        void ApplyToWorld(flecs::world& world) const;

        /// Read-only access to registered component descriptors.
        const std::vector<ComponentDescriptor>& GetComponentDescriptors() const
        {
            return m_components;
        }

        /// Read-only access to registered system descriptors.
        const std::vector<SystemDescriptor>& GetSystems() const
        {
            return m_systems.GetDescriptors();
        }

        /// Read-only access to registered state descriptors.
        const std::vector<StateDescriptor>& GetStateDescriptors() const
        {
            return m_states.GetDescriptors();
        }

        /// Returns the initial state name (empty if none was set).
        const std::string& GetInitialState() const
        {
            return m_states.GetInitial();
        }

        /// Read-only access to registered tag descriptors.
        const std::vector<TagDescriptor>& GetRegisteredTags() const
        {
            return m_tags.GetDescriptors();
        }

        /// Read-only access to registered tag file paths.
        const std::vector<std::string>& GetTagFiles() const
        {
            return m_tags.GetFiles();
        }

        /// Subsystem factory entry for SubsystemCollection integration.
        struct SubsystemFactoryEntry
        {
            std::type_index Type;
            ::Wayfinder::SubsystemCollection<::Wayfinder::GameSubsystem>::FactoryFn Factory;
            ::Wayfinder::SubsystemCollection<::Wayfinder::GameSubsystem>::PredicateFn Predicate = nullptr;
        };

        /// Read-only access to plugin-registered subsystem factories.
        const std::vector<SubsystemFactoryEntry>& GetSubsystemFactories() const
        {
            return m_subsystemFactories;
        }

        /// Read-only access to the project descriptor.
        const ::Wayfinder::ProjectDescriptor& GetProject() const;

        /**
         * @brief Read-only access to the engine configuration.
         * @return A const reference to the EngineConfig.
         */
        const ::Wayfinder::EngineConfig& GetConfig() const;

    private:
        const ::Wayfinder::ProjectDescriptor& m_project;
        const ::Wayfinder::EngineConfig& m_config;
        std::vector<std::unique_ptr<Plugin>> m_plugins;
        SystemRegistrar m_systems;
        StateRegistrar m_states;
        TagRegistrar m_tags;
        std::vector<ComponentDescriptor> m_components;
        std::vector<GlobalDescriptor> m_globals;
        std::vector<SubsystemFactoryEntry> m_subsystemFactories;
    };

} // namespace Wayfinder::Plugins
