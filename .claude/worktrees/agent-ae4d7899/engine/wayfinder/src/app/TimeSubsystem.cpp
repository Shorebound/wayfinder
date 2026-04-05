#include "TimeSubsystem.h"

#include "app/ConfigService.h"
#include "app/EngineConfig.h"
#include "app/EngineContext.h"
#include "core/Log.h"
#include "platform/Time.h"

#include <cassert>

namespace Wayfinder
{
    TimeSubsystem::~TimeSubsystem()
    {
        Shutdown();
    }

    auto TimeSubsystem::Initialise(EngineContext& context) -> Result<void>
    {
        Log::Info(LogEngine, "TimeSubsystem: Initialising");

        const auto& configService = context.GetAppSubsystem<ConfigService>();
        const auto& engineConfig = configService.Get<EngineConfig>();

        m_time = Time::Create(engineConfig.Backends.Platform);
        if (!m_time)
        {
            Log::Error(LogEngine, "TimeSubsystem: Failed to create Time");
            return MakeError("TimeSubsystem: Failed to create Time");
        }

        Log::Info(LogEngine, "TimeSubsystem: Initialised");
        return {};
    }

    void TimeSubsystem::Shutdown()
    {
        if (!m_time)
        {
            return;
        }

        Log::Info(LogEngine, "TimeSubsystem: Shutting down");
        m_time.reset();
    }

    auto TimeSubsystem::GetTime() -> Time&
    {
        assert(m_time && "GetTime called before Initialise or after Shutdown");
        return *m_time;
    }

    auto TimeSubsystem::GetDeltaTime() const -> float
    {
        return m_time ? m_time->GetDeltaTime() : 0.0f;
    }

} // namespace Wayfinder
