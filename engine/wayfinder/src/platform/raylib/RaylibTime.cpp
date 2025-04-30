#include "RaylibTime.h"
#include "raylib.h"

namespace Wayfinder
{
    std::unique_ptr<Time> Time::Create()
    {
        return std::make_unique<RaylibTime>();
    }

    RaylibTime::RaylibTime() : m_deltaTime(0.0f), m_elapsedTime(0.0f), m_StartTime(GetTime())
    {
    }

    void RaylibTime::Update()
    {
        m_deltaTime = GetFrameTime();
        m_elapsedTime += m_deltaTime;
    }

    float RaylibTime::GetDeltaTime() const
    {
        return m_deltaTime;
    }

    float RaylibTime::GetElapsedTime() const
    {
        return m_elapsedTime;
    }

    double RaylibTime::GetTimeSinceStartup() const
    {
        return GetTime() - m_StartTime;
    }
}
