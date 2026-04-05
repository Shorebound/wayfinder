#pragma once

#include "app/AppSubsystem.h"
#include "core/Result.h"
#include "wayfinder_exports.h"

#include <cstdint>

namespace Wayfinder
{
    class EngineContext;

    /**
     * @brief Direct SDL3 time management as an AppSubsystem.
     *
     * Computes frame delta time and elapsed time using SDL performance
     * counters. Replaces the three-tier Time (abstract) + SDL3Time (impl) +
     * TimeSubsystem (wrapper) with a single concrete type. Always active
     * (no required capabilities).
     */
    class WAYFINDER_API SDLTimeSubsystem final : public AppSubsystem
    {
    public:
        SDLTimeSubsystem() = default;
        ~SDLTimeSubsystem() override = default;

        [[nodiscard]] auto Initialise(EngineContext& context) -> Result<void> override;
        void Shutdown() override;

        /// Call at the start of each frame to compute delta time.
        void Update();

        [[nodiscard]] auto GetDeltaTime() const -> float
        {
            return m_deltaTime;
        }
        [[nodiscard]] auto GetElapsedTime() const -> float
        {
            return m_elapsedTime;
        }
        [[nodiscard]] auto GetTimeSinceStartup() const -> double;

    private:
        uint64_t m_lastTicks = 0;
        uint64_t m_startTicks = 0;
        uint64_t m_frequency = 0;
        float m_deltaTime = 0.0f;
        float m_elapsedTime = 0.0f;
    };

} // namespace Wayfinder
