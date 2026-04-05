#pragma once

#include "app/AppSubsystem.h"
#include "core/Result.h"

namespace Wayfinder
{
    /**
     * @brief Deterministic time subsystem for headless integration tests.
     *
     * Provides a fixed delta time (default 1/60s) so tests can control frame-by-frame
     * advancement without any SDL dependency. Header-only; not shipped with the engine.
     *
     * Usage: register via a test plugin or SubsystemRegistry<AppSubsystem> when
     * booting Application in headless mode.
     */
    class FixedTimeSubsystem final : public AppSubsystem
    {
    public:
        static constexpr float DEFAULT_DT = 1.0f / 60.0f;

        [[nodiscard]] auto Initialise(EngineContext&) -> Result<void> override
        {
            return {};
        }

        void Shutdown() override {}

        void Update()
        {
            m_elapsedTime += m_deltaTime;
        }

        [[nodiscard]] auto GetDeltaTime() const -> float
        {
            return m_deltaTime;
        }
        [[nodiscard]] auto GetElapsedTime() const -> float
        {
            return m_elapsedTime;
        }

        void SetDeltaTime(float dt)
        {
            m_deltaTime = dt;
        }

    private:
        float m_deltaTime = DEFAULT_DT;
        float m_elapsedTime = 0.0f;
    };

} // namespace Wayfinder
