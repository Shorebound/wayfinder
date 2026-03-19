#pragma once

namespace Wayfinder
{
    class Input;
    class Time;
    class Window;
    struct EngineConfig;
    struct ProjectDescriptor;

    struct EngineContext
    {
        Window& window;
        Input& input;
        Time& time;
        const EngineConfig& config;
        const ProjectDescriptor& project;
    };

} // namespace Wayfinder
