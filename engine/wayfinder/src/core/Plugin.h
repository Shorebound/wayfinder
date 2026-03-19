#pragma once

#include "wayfinder_exports.h"

namespace Wayfinder
{
    class ModuleRegistry;

    /// A Plugin groups related ECS systems, components, and globals.
    ///
    /// Plugins are the primary unit of engine extensibility. A game module
    /// creates plugins and adds them via ModuleRegistry::AddPlugin<T>()
    /// during Module::Register(). Each plugin declares its registrations
    /// in Build(), which the engine calls once.
    class WAYFINDER_API Plugin
    {
    public:
        virtual ~Plugin() = default;

        /// Declare systems, components, and globals on the registry.
        virtual void Build(ModuleRegistry& registry) = 0;
    };

} // namespace Wayfinder
