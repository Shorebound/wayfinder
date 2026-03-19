#pragma once

#include "wayfinder_exports.h"

#include <functional>
#include <string>
#include <vector>

namespace flecs
{
    struct world;
}

namespace Wayfinder
{
    struct EngineConfig;
    struct ProjectDescriptor;

    /// Collects game-specific ECS registrations during Module::Register().
    ///
    /// This is a descriptor store, not a live world facade. Game modules
    /// declare their systems/components here, and the engine replays those
    /// declarations into every new flecs::world (i.e. every scene).
    class WAYFINDER_API ModuleRegistry
    {
    public:
        using SystemFactory = std::function<void(flecs::world&)>;

        struct SystemDescriptor
        {
            std::string Name;
            SystemFactory Factory;
        };

        ModuleRegistry(const ProjectDescriptor& project,
                       const EngineConfig& config);

        /// Register a named ECS system factory.  The factory will be called
        /// once for every new flecs::world the engine creates.
        void RegisterSystem(std::string name, SystemFactory factory);

        /// Replay all registered system factories into the given world.
        /// Called by Scene::Initialize after core components/systems.
        void ApplyToWorld(flecs::world& world) const;

        /// Read-only access to the project descriptor.
        const ProjectDescriptor& GetProject() const;

        /// Read-only access to engine configuration.
        const EngineConfig& GetConfig() const;

    private:
        const ProjectDescriptor& m_project;
        const EngineConfig& m_config;
        std::vector<SystemDescriptor> m_systems;
    };

} // namespace Wayfinder
