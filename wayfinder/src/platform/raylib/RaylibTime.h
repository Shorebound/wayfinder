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
        float m_DeltaTime;
        float m_ElapsedTime;
        double m_StartTime;
    };
}
