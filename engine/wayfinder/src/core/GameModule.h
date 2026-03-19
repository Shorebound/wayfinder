#pragma once

#include "wayfinder_exports.h"

#include <memory>

namespace Wayfinder
{
    struct EngineContext;
    struct ProjectDescriptor;

    /// Base class for game-specific behaviour.
    ///
    /// Games implement a derived class and supply it via CreateGameModule().
    /// The engine owns the module and invokes the virtual hooks at the
    /// corresponding lifecycle points. Override only what you need.
    class WAYFINDER_API GameModule
    {
    public:
        virtual ~GameModule() = default;

        /// Called after the project descriptor has been parsed but before engine
        /// subsystems are initialised.
        virtual void OnProjectLoaded(const ProjectDescriptor& /*project*/) {}

        /// Called after all engine subsystems (window, renderer, ECS, etc.) have
        /// been initialised and the boot scene has been loaded.
        virtual void OnInitialize(const EngineContext& /*ctx*/) {}

        /// Called once per frame with the current delta-time.
        virtual void OnUpdate(float /*deltaTime*/) {}

        /// Called during engine shutdown, before subsystems are torn down.
        virtual void OnShutdown() {}
    };

    /// Games must define this function. It is called by the engine entry point
    /// to obtain the game's module instance.
    extern std::unique_ptr<GameModule> CreateGameModule();

} // namespace Wayfinder
