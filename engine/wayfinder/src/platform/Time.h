#pragma once

#include "../core/BackendConfig.h"

namespace Wayfinder
{
    // Interface for time management
    class WAYFINDER_API Time
    {
    public:
        virtual ~Time() = default;

        virtual void Update() = 0;
        virtual float GetDeltaTime() const = 0;
        virtual float GetElapsedTime() const = 0;
        virtual double GetTimeSinceStartup() const = 0;

        static std::unique_ptr<Time> Create(PlatformBackend backend = PlatformBackend::SDL3);
    };
}
