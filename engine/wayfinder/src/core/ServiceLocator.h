#pragma once

#include "BackendConfig.h"

namespace Wayfinder
{
    class Input;
    class Time;

    // Service locator for platform services (input, time).
    // Rendering is owned by Application → RenderDevice → Renderer.
    class WAYFINDER_API ServiceLocator
    {
    public:
        static void Initialize(const BackendConfig& config = {});
        static void Shutdown();

        static Input& GetInput();
        static Time& GetTime();

    private:
        static std::unique_ptr<Input> s_input;
        static std::unique_ptr<Time> s_time;
    };
}
