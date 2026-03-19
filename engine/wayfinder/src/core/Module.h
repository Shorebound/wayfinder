#pragma once

#include "wayfinder_exports.h"

#include <memory>

namespace Wayfinder
{
    class ModuleRegistry;

    /// Base class for game-specific modules.
    ///
    /// A Module declares game-specific ECS systems, components, and other
    /// extensions via ModuleRegistry. These declarations are replayed into
    /// every new scene world the engine creates — the module never touches a
    /// flecs::world directly. Per-frame game logic belongs in ECS systems,
    /// not in module callbacks.
    ///
    /// Games implement a derived class and supply it via CreateModule().
    /// The engine owns the module and invokes the virtual hooks at the
    /// corresponding lifecycle points.
    class WAYFINDER_API Module
    {
    public:
        virtual ~Module() = default;

        /// Declare game-specific ECS systems, components, and other extensions.
        /// Called once at startup before any scene is loaded. The registered
        /// factories will be replayed into every new flecs::world.
        virtual void Register(ModuleRegistry& registry) = 0;

        /// Called once after Register(), before the first frame.
        /// Use for one-time, non-ECS setup (e.g. loading save data).
        virtual void OnStartup() {}

        /// Called during engine shutdown, before subsystems are torn down.
        virtual void OnShutdown() {}
    };

    /// Games must define this function. It is called by the engine entry point
    /// to obtain the game's module instance.
    extern std::unique_ptr<Module> CreateModule();

} // namespace Wayfinder
