#pragma once

#include "wayfinder_exports.h"

namespace Wayfinder::Plugins
{
    class PluginRegistry;

    /**
     * @brief Composable unit of ECS systems, components, and globals.
     *
     * Plugins are the primary unit of engine extensibility. Register them via
     * PluginRegistry::AddPlugin<T>() or AddPlugin(std::unique_ptr<Plugin>) during
     * initialisation. Each plugin declares its registrations in Build(), which
     * the engine calls once. Optional OnStartup / OnShutdown run after the world
     * is ready and during shutdown respectively.
     */
    class WAYFINDER_API Plugin
    {
    public:
        virtual ~Plugin() = default;

        /// Declare systems, components, and globals on the registry.
        virtual void Build(PluginRegistry& registry) = 0;

        /// Called once after Game initialisation completes, before the first frame.
        virtual void OnStartup() {}

        /// Called during engine shutdown, before subsystems are torn down.
        virtual void OnShutdown() {}
    };

} // namespace Wayfinder::Plugins
