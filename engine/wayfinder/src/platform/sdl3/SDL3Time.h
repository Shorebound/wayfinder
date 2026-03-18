#pragma once
#include "../Time.h"

namespace Wayfinder
{
    class SDL3Time : public Time
    {
    public:
        SDL3Time();
        ~SDL3Time() override = default;

        void Update() override;
        float GetDeltaTime() const override;
        float GetElapsedTime() const override;
        double GetTimeSinceStartup() const override;

    private:
        uint64_t m_lastTicks;
        uint64_t m_startTicks;
        float m_deltaTime;
        float m_elapsedTime;
    };
}
