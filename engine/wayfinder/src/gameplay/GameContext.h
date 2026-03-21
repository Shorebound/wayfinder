#pragma once

namespace Wayfinder
{
    class ModuleRegistry;
    struct ProjectDescriptor;

    /**
     * @brief Lightweight context for Game initialisation.
     *
     * Carries only what Game actually needs — project identity and optional
     * module registration.  Platform services (Window, Input, Time) and
     * rendering infrastructure are owned by EngineRuntime and are not
     * exposed to Game.
     */
    struct GameContext
    {
        const ProjectDescriptor& project;
        const ModuleRegistry* moduleRegistry = nullptr;
    };

} // namespace Wayfinder
