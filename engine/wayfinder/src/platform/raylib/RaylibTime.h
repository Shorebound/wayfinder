#pragma once
#include "../Time.h"

namespace Wayfinder
{
    class RaylibTime : public Time
    {
    public:
        RaylibTime();
        virtual ~RaylibTime() = default;

        void Update() override;
        float GetDeltaTime() const override;
        float GetElapsedTime() const override;
        double GetTimeSinceStartup() const override;

    private:
        float m_deltaTime;
        float m_elapsedTime;
        double m_StartTime;
    };
}
