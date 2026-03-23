#pragma once

#include "platform/Time.h"

namespace Wayfinder
{
    class NullTime final : public Time
    {
    public:
        void Update() override
        {
            m_elapsed += m_fixedDelta;
        }

        float GetDeltaTime() const override
        {
            return m_fixedDelta;
        }
        float GetElapsedTime() const override
        {
            return m_elapsed;
        }
        double GetTimeSinceStartup() const override
        {
            return static_cast<double>(m_elapsed);
        }

    private:
        float m_fixedDelta = 1.0f / 60.0f;
        float m_elapsed = 0.0f;
    };

} // namespace Wayfinder
