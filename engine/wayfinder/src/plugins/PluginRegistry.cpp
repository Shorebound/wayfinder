#include "PluginRegistry.h"
#include "core/Log.h"

#include "ecs/Flecs.h"

namespace Wayfinder
{
    PluginRegistry::PluginRegistry(const ProjectDescriptor& project, const EngineConfig& config) : m_project(project), m_config(config) {}

    void PluginRegistry::AddPlugin(std::unique_ptr<Plugin> plugin)
    {
        if (!plugin)
        {
            WAYFINDER_ERROR(LogEngine, "PluginRegistry: AddPlugin received null plugin");
            return;
        }
        plugin->Build(*this);
        m_plugins.push_back(std::move(plugin));
    }

    void PluginRegistry::NotifyStartup()
    {
        for (auto& plugin : m_plugins)
        {
            plugin->OnStartup();
        }
    }

    void PluginRegistry::NotifyShutdown()
    {
        for (auto it = m_plugins.rbegin(); it != m_plugins.rend(); ++it)
        {
            (*it)->OnShutdown();
        }
    }

    void PluginRegistry::RegisterSystem(std::string name, SystemFactory factory, RunCondition condition, std::vector<std::string> after, std::vector<std::string> before)
    {
        m_systems.Register(std::move(name), std::move(factory), std::move(condition), std::move(after), std::move(before));
    }

    void PluginRegistry::RegisterComponent(ComponentDescriptor descriptor)
    {
        WAYFINDER_INFO(LogEngine, "PluginRegistry: registered component '{}'", descriptor.Key);
        m_components.push_back(std::move(descriptor));
    }

    void PluginRegistry::RegisterGlobal(std::string name, GlobalFactory factory)
    {
        WAYFINDER_INFO(LogEngine, "PluginRegistry: registered global '{}'", name);
        m_globals.push_back({.Name = std::move(name), .Factory = std::move(factory)});
    }

    void PluginRegistry::RegisterState(StateDescriptor descriptor)
    {
        m_states.Register(std::move(descriptor));
    }

    void PluginRegistry::SetInitialState(std::string stateName)
    {
        m_states.SetInitial(std::move(stateName));
    }

    GameplayTag PluginRegistry::RegisterTag(std::string tagName, std::string comment)
    {
        GameplayTag tag = GameplayTag::FromName(tagName);
        m_tags.Register({.Name = std::move(tagName), .Comment = std::move(comment)});
        return tag;
    }

    void PluginRegistry::RegisterTagFile(std::string relativePath)
    {
        m_tags.AddFile(std::move(relativePath));
    }

    void PluginRegistry::ApplyComponentRegisterFns(flecs::world& world) const
    {
        for (const auto& desc : m_components)
        {
            if (desc.RegisterFn)
            {
                desc.RegisterFn(world);
            }
        }
    }

    void PluginRegistry::ApplyToWorld(flecs::world& world) const
    {
        // Component registration is handled by RuntimeComponentRegistry,
        // which merges core + game entries. Here we only apply globals and systems.
        for (const auto& desc : m_globals)
        {
            desc.Factory(world);
        }

        m_systems.ApplyToWorld(world);
    }

    const ProjectDescriptor& PluginRegistry::GetProject() const
    {
        return m_project;
    }

    const EngineConfig& PluginRegistry::GetConfig() const
    {
        return m_config;
    }

} // namespace Wayfinder
