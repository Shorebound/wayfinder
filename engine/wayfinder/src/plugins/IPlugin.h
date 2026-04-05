#pragma once

#include "PluginDescriptor.h"

namespace Wayfinder
{
    class AppBuilder;

    /**
     * @brief V2 plugin interface with Build-only pattern.
     *
     * Plugins are the primary unit of engine extensibility in the v2
     * architecture. Each plugin declares its registrations in Build(),
     * which receives an AppBuilder for registering subsystems, states,
     * overlays, and other components.
     *
     * Unlike the v1 Plugin (in Wayfinder::Plugins), this interface has
     * no OnStartup/OnShutdown -- lifecycle is managed by the subsystems
     * and states that the plugin registers.
     */
    class IPlugin
    {
    public:
        virtual ~IPlugin() = default;

        /// Declare subsystems, states, overlays, and other components.
        virtual void Build(AppBuilder& builder) = 0;

        /// Declare plugin metadata (name, dependencies).
        /// Override to declare dependencies on other plugins.
        /// Default returns an empty descriptor (no name, no dependencies).
        [[nodiscard]] virtual auto Describe() const -> PluginDescriptor
        {
            return {};
        }
    };

} // namespace Wayfinder
