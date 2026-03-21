#pragma once

namespace Wayfinder
{
    class Input;
    class Time;
    class Window;
    struct EngineConfig;
    struct ProjectDescriptor;

    /**
     * @brief Non-owning reference bundle for engine platform and rendering services.
     *
     * Built by EngineRuntime via BuildContext().  Intended for external consumers
     * such as the editor (Cartographer) that need access to the live runtime
     * services without owning them.
     */
    struct EngineContext
    {
        Window& window;
        Input& input;
        Time& time;
        const EngineConfig& config;
        const ProjectDescriptor& project;
    };

} // namespace Wayfinder
