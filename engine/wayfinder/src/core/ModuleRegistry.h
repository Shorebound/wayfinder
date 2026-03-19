#pragma once

#include "GameState.h"
#include "Plugin.h"
#include "Subsystem.h"
#include "wayfinder_exports.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <typeindex>
#include <vector>

#include <toml++/toml.hpp>

namespace flecs
{
    struct world;
}

namespace Wayfinder
{
    class Entity;
    struct EngineConfig;
    struct ProjectDescriptor;

    /// Collects game-specific ECS registrations during Module::Register().
    ///
    /// This is a descriptor store, not a live world facade. Game modules
    /// declare their systems/components here, and the engine applies those
    /// declarations once into the persistent flecs::world at startup.
    class WAYFINDER_API ModuleRegistry
    {
    public:
        using SystemFactory = std::function<void(flecs::world&)>;
        using ComponentRegisterFn = void(*)(flecs::world& world);
        using ComponentApplyFn = void(*)(const toml::table& componentTable, Entity& entity);
        using ComponentSerializeFn = void(*)(const Entity& entity, toml::table& componentTables);
        using ComponentValidateFn = bool(*)(const toml::table& componentTable, std::string& error);
        using GlobalFactory = std::function<void(flecs::world&)>;

        struct SystemDescriptor
        {
            std::string Name;
            SystemFactory Factory;
            RunCondition Condition;
            std::vector<std::string> After;  ///< Names of systems this must run after.
            std::vector<std::string> Before; ///< Names of systems this must run before.
        };

        /// Describes a serializable ECS component for scene authoring.
        struct ComponentDescriptor
        {
            std::string Key;
            ComponentRegisterFn RegisterFn = nullptr;
            ComponentApplyFn ApplyFn = nullptr;
            ComponentSerializeFn SerializeFn = nullptr;
            ComponentValidateFn ValidateFn = nullptr;
        };

        /// Describes a typed world singleton (global data).
        struct GlobalDescriptor
        {
            std::string Name;
            GlobalFactory Factory;
        };

        /// Describes a named game state with enter/exit lifecycle callbacks.
        struct StateDescriptor
        {
            std::string Name;
            std::function<void(flecs::world&)> OnEnter;
            std::function<void(flecs::world&)> OnExit;
        };

        /// Describes a code-registered gameplay tag.
        struct TagDescriptor
        {
            std::string Name;
            std::string Comment;
        };

        ModuleRegistry(const ProjectDescriptor& project,
                       const EngineConfig& config);

        /// Add a plugin. The plugin's Build() is called immediately.
        template <typename T>
        void AddPlugin()
        {
            auto plugin = std::make_unique<T>();
            plugin->Build(*this);
            m_plugins.push_back(std::move(plugin));
        }

        /// Register a named ECS system factory.  The factory will be called
        /// once when the engine creates its persistent flecs::world.
        /// An optional RunCondition controls whether the system is active.
        /// Optional After/Before lists declare ordering relative to other systems.
        void RegisterSystem(std::string name, SystemFactory factory,
                            RunCondition condition = {},
                            std::vector<std::string> after = {},
                            std::vector<std::string> before = {});

        /// Register a serializable component for scene authoring.
        void RegisterComponent(ComponentDescriptor descriptor);

        /// Register a typed world singleton (global data).
        void RegisterGlobal(std::string name, GlobalFactory factory);

        /// Register a named game state with optional enter/exit callbacks.
        void RegisterState(StateDescriptor descriptor);

        /// Set the initial game state. Game will transition to this state
        /// during initialization, after all registrations are applied.
        void SetInitialState(std::string stateName);

        /// Register a gameplay tag name. Returns a GameplayTag that can be
        /// captured and used immediately (e.g. passed to HasTag run conditions).
        GameplayTag RegisterTag(std::string tagName, std::string comment = {});

        /// Register a tag definition file to be loaded at startup.
        /// Path is relative to the project's config directory.
        void RegisterTagFile(std::string relativePath);

        /// Register a game subsystem type. It will be created automatically
        /// during Game initialisation alongside engine-core subsystems.
        template <typename T>
        void RegisterSubsystem()
        {
            static_assert(std::is_base_of_v<GameSubsystem, T>,
                          "T must derive from GameSubsystem");
            m_subsystemFactories.push_back(
                {std::type_index(typeid(T)),
                 []() -> std::unique_ptr<GameSubsystem> { return std::make_unique<T>(); }});
        }

        /// Apply all registered system factories into the given world.
        /// Called once by Game::InitializeWorld after core ECS setup.
        void ApplyToWorld(flecs::world& world) const;

        /// Read-only access to registered component descriptors.
        const std::vector<ComponentDescriptor>& GetComponentDescriptors() const { return m_components; }

        /// Read-only access to registered system descriptors.
        const std::vector<SystemDescriptor>& GetSystems() const { return m_systems; }

        /// Read-only access to registered state descriptors.
        const std::vector<StateDescriptor>& GetStateDescriptors() const { return m_states; }

        /// Returns the initial state name (empty if none was set).
        const std::string& GetInitialState() const { return m_initialState; }

        /// Read-only access to registered tag descriptors.
        const std::vector<TagDescriptor>& GetRegisteredTags() const { return m_tags; }

        /// Read-only access to registered tag file paths.
        const std::vector<std::string>& GetTagFiles() const { return m_tagFiles; }

        /// Subsystem factory entry for SubsystemCollection integration.
        using SubsystemFactoryEntry = std::pair<std::type_index,
                                                SubsystemCollection<GameSubsystem>::FactoryFn>;

        /// Read-only access to module-registered subsystem factories.
        const std::vector<SubsystemFactoryEntry>& GetSubsystemFactories() const { return m_subsystemFactories; }

        /// Read-only access to the project descriptor.
        const ProjectDescriptor& GetProject() const;

        /// Read-only access to engine configuration.
        const EngineConfig& GetConfig() const;

    private:
        const ProjectDescriptor& m_project;
        const EngineConfig& m_config;
        std::vector<std::unique_ptr<Plugin>> m_plugins;
        std::vector<SystemDescriptor> m_systems;
        std::vector<ComponentDescriptor> m_components;
        std::vector<GlobalDescriptor> m_globals;
        std::vector<StateDescriptor> m_states;
        std::vector<TagDescriptor> m_tags;
        std::vector<std::string> m_tagFiles;
        std::vector<SubsystemFactoryEntry> m_subsystemFactories;
        std::string m_initialState;
    };

} // namespace Wayfinder
