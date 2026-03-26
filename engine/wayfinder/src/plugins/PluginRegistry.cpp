#include "PluginRegistry.h"
#include "core/Log.h"

#include "ecs/Flecs.h"

#include <string>

namespace Wayfinder::Plugins
{
    PluginRegistry::PluginRegistry(const ::Wayfinder::ProjectDescriptor& project, const ::Wayfinder::EngineConfig& config) : m_project(project), m_config(config) {}

    void PluginRegistry::AddPlugin(std::unique_ptr<Plugin> plugin)
    {
        if (!plugin)
        {
            WAYFINDER_ERROR(LogEngine, "PluginRegistry: AddPlugin received null plugin");
            return;
        }
        m_plugins.push_back(std::move(plugin));
        m_plugins.back()->Build(*this);
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

    void PluginRegistry::RegisterSystem(std::string name, SystemFactory factory, ::Wayfinder::RunCondition condition, std::vector<std::string> after, std::vector<std::string> before)
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
        if (!factory)
        {
            WAYFINDER_ERROR(LogEngine, "PluginRegistry: empty factory for global '{}' — registration rejected", name);
            return;
        }

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

    ::Wayfinder::GameplayTag PluginRegistry::RegisterTag(const std::string_view tagName, const std::string_view comment)
    {
        const ::Wayfinder::GameplayTag tag = ::Wayfinder::GameplayTag::FromName(tagName);
        m_tags.Register({.Name = std::string(tagName), .Comment = std::string(comment)});
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

    const ::Wayfinder::ProjectDescriptor& PluginRegistry::GetProject() const
    {
        return m_project;
    }

    const ::Wayfinder::EngineConfig& PluginRegistry::GetConfig() const
    {
        return m_config;
    }

} // namespace Wayfinder::Plugins
