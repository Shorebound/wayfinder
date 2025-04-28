#include "RaylibTime.h"
#include "raylib.h"

namespace Wayfinder
{
    std::unique_ptr<Time> Time::Create()
    {
        return std::make_unique<RaylibTime>();
    }

    RaylibTime::RaylibTime()
        : m_DeltaTime(0.0f), m_ElapsedTime(0.0f), m_StartTime(GetTime())
    {
    }

    void RaylibTime::Update()
    {
        m_DeltaTime = GetFrameTime();
        m_ElapsedTime += m_DeltaTime;
    }

    float RaylibTime::GetDeltaTime() const
    {
        return m_DeltaTime;
    }

    float RaylibTime::GetElapsedTime() const
    {
        return m_ElapsedTime;
    }

    double RaylibTime::GetTimeSinceStartup() const
    {
        return GetTime() - m_StartTime;
    }
}
