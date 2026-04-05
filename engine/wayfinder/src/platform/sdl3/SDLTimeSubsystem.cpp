#include "SDLTimeSubsystem.h"

#include "core/Log.h"

#include <SDL3/SDL.h>

namespace Wayfinder
{
    auto SDLTimeSubsystem::Initialise([[maybe_unused]] EngineContext& context) -> Result<void>
    {
        m_frequency = SDL_GetPerformanceFrequency();
        m_lastTicks = SDL_GetPerformanceCounter();
        m_startTicks = m_lastTicks;
        m_deltaTime = 0.0f;
        m_elapsedTime = 0.0f;

        Log::Info(LogEngine, "SDLTimeSubsystem: Initialised (frequency={})", m_frequency);
        return {};
    }

    void SDLTimeSubsystem::Shutdown()
    {
        Log::Info(LogEngine, "SDLTimeSubsystem: Shutting down");
    }

    void SDLTimeSubsystem::Update()
    {
        const uint64_t now = SDL_GetPerformanceCounter();
        m_deltaTime = static_cast<float>(static_cast<double>(now - m_lastTicks) / static_cast<double>(m_frequency));
        m_elapsedTime += m_deltaTime;
        m_lastTicks = now;
    }

    auto SDLTimeSubsystem::GetTimeSinceStartup() const -> double
    {
        const uint64_t now = SDL_GetPerformanceCounter();
        return static_cast<double>(now - m_startTicks) / static_cast<double>(m_frequency);
    }

} // namespace Wayfinder
