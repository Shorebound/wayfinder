#include "ModuleRegistry.h"
#include "Log.h"

#include <flecs.h>

namespace Wayfinder
{
    ModuleRegistry::ModuleRegistry(const ProjectDescriptor& project,
                                   const EngineConfig& config)
        : m_project(project), m_config(config) {}

    void ModuleRegistry::RegisterSystem(std::string name, SystemFactory factory, RunCondition condition)
    {
        WAYFINDER_INFO(LogEngine, "ModuleRegistry: registered system '{}'{}", name,
                       condition ? " (conditioned)" : "");
        m_systems.push_back({std::move(name), std::move(factory), std::move(condition)});
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
        WAYFINDER_INFO(LogEngine, "ModuleRegistry: registered state '{}'", descriptor.Name);
        m_states.push_back(std::move(descriptor));
    }

    void ModuleRegistry::SetInitialState(std::string stateName)
    {
        m_initialState = std::move(stateName);
    }

    GameplayTag ModuleRegistry::RegisterTag(std::string tagName, std::string comment)
    {
        WAYFINDER_INFO(LogEngine, "ModuleRegistry: registered tag '{}'", tagName);
        GameplayTag tag{tagName};
        m_tags.push_back({std::move(tagName), std::move(comment)});
        return tag;
    }

    void ModuleRegistry::RegisterTagFile(std::string relativePath)
    {
        WAYFINDER_INFO(LogEngine, "ModuleRegistry: registered tag file '{}'", relativePath);
        m_tagFiles.push_back(std::move(relativePath));
    }

    void ModuleRegistry::ApplyToWorld(flecs::world& world) const
    {
        // Component registration is handled by RuntimeComponentRegistry,
        // which merges core + game entries. Here we only apply globals and systems.
        for (const auto& desc : m_globals)
        {
            desc.Factory(world);
        }

        for (const auto& desc : m_systems)
        {
            desc.Factory(world);
        }
    }

    const ProjectDescriptor& ModuleRegistry::GetProject() const { return m_project; }

    const EngineConfig& ModuleRegistry::GetConfig() const { return m_config; }

} // namespace Wayfinder
