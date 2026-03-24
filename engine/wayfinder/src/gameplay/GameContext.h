#pragma once

namespace Wayfinder::Plugins
{
    class PluginRegistry;
}

namespace Wayfinder
{
    struct ProjectDescriptor;

    /**
     * @brief Lightweight context for Game initialisation.
     *
     * Carries only what Game actually needs — project identity and plugin
     * registration.  Platform services (Window, Input, Time) and
     * rendering infrastructure are owned by EngineRuntime and are not
     * exposed to Game.
     */
    struct GameContext
    {
        const ProjectDescriptor& project;
        const Plugins::PluginRegistry& pluginRegistry;
    };

} // namespace Wayfinder
