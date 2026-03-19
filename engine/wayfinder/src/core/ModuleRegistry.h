#pragma once

#include "Plugin.h"
#include "wayfinder_exports.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
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
        void RegisterSystem(std::string name, SystemFactory factory);

        /// Register a serializable component for scene authoring.
        void RegisterComponent(ComponentDescriptor descriptor);

        /// Register a typed world singleton (global data).
        void RegisterGlobal(std::string name, GlobalFactory factory);

        /// Apply all registered system factories into the given world.
        /// Called once by Game::InitializeWorld after core ECS setup.
        void ApplyToWorld(flecs::world& world) const;

        /// Read-only access to registered component descriptors.
        const std::vector<ComponentDescriptor>& GetComponentDescriptors() const { return m_components; }

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
    };

} // namespace Wayfinder
