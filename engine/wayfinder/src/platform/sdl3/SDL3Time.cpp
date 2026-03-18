#include "SDL3Time.h"

#include <SDL3/SDL.h>

namespace Wayfinder
{
    std::unique_ptr<Time> Time::Create(PlatformBackend backend)
    {
        switch (backend)
        {
        case PlatformBackend::SDL3:
            return std::make_unique<SDL3Time>();
        }

        return nullptr;
    }

    SDL3Time::SDL3Time()
        : m_lastTicks(SDL_GetPerformanceCounter())
        , m_startTicks(m_lastTicks)
        , m_deltaTime(0.0f)
        , m_elapsedTime(0.0f)
    {
    }

    void SDL3Time::Update()
    {
        const uint64_t now = SDL_GetPerformanceCounter();
        const uint64_t freq = SDL_GetPerformanceFrequency();
        m_deltaTime = static_cast<float>(static_cast<double>(now - m_lastTicks) / static_cast<double>(freq));
        m_elapsedTime += m_deltaTime;
        m_lastTicks = now;
    }

    float SDL3Time::GetDeltaTime() const
    {
        return m_deltaTime;
    }

    float SDL3Time::GetElapsedTime() const
    {
        return m_elapsedTime;
    }

    double SDL3Time::GetTimeSinceStartup() const
    {
        const uint64_t now = SDL_GetPerformanceCounter();
        const uint64_t freq = SDL_GetPerformanceFrequency();
        return static_cast<double>(now - m_startTicks) / static_cast<double>(freq);
    }
}
