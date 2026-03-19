#include "ModuleRegistry.h"
#include "Log.h"

#include <flecs.h>

namespace Wayfinder
{
    ModuleRegistry::ModuleRegistry(const ProjectDescriptor& project,
                                   const EngineConfig& config)
        : m_project(project), m_config(config) {}

    void ModuleRegistry::RegisterSystem(std::string name, SystemFactory factory,
                                         RunCondition condition,
                                         std::vector<std::string> after,
                                         std::vector<std::string> before)
    {
        m_systems.Register(std::move(name), std::move(factory), std::move(condition),
                           std::move(after), std::move(before));
    }

    void ModuleRegistry::RegisterComponent(ComponentDescriptor descriptor)
    {
        WAYFINDER_INFO(LogEngine, "ModuleRegistry: registered component '{}'", descriptor.Key);
        m_components.push_back(std::move(descriptor));
    }

    void ModuleRegistry::RegisterGlobal(std::string name, GlobalFactory factory)
    {
        WAYFINDER_INFO(LogEngine, "ModuleRegistry: registered global '{}'", name);
        m_globals.push_back({std::move(name), std::move(factory)});
    }

    void ModuleRegistry::RegisterState(StateDescriptor descriptor)
    {
        m_states.Register(std::move(descriptor));
    }

    void ModuleRegistry::SetInitialState(std::string stateName)
    {
        m_states.SetInitial(std::move(stateName));
    }

    GameplayTag ModuleRegistry::RegisterTag(std::string tagName, std::string comment)
    {
        GameplayTag tag{tagName};
        m_tags.Register({std::move(tagName), std::move(comment)});
        return tag;
    }

    void ModuleRegistry::RegisterTagFile(std::string relativePath)
    {
        m_tags.AddFile(std::move(relativePath));
    }

    void ModuleRegistry::ApplyToWorld(flecs::world& world) const
    {
        // Component registration is handled by RuntimeComponentRegistry,
        // which merges core + game entries. Here we only apply globals and systems.
        for (const auto& desc : m_globals)
        {
            desc.Factory(world);
        }

        m_systems.ApplyToWorld(world);
    }

    const ProjectDescriptor& ModuleRegistry::GetProject() const { return m_project; }

} // namespace Wayfinder
