#include "InputSubsystem.h"

#include "app/ConfigService.h"
#include "app/EngineConfig.h"
#include "app/EngineContext.h"
#include "core/Log.h"
#include "platform/Input.h"

#include <cassert>

namespace Wayfinder
{
    InputSubsystem::~InputSubsystem()
    {
        Shutdown();
    }

    auto InputSubsystem::Initialise(EngineContext& context) -> Result<void>
    {
        Log::Info(LogEngine, "InputSubsystem: Initialising");

        const auto& configService = context.GetAppSubsystem<ConfigService>();
        const auto& engineConfig = configService.Get<EngineConfig>();

        m_input = Input::Create(engineConfig.Backends.Platform);
        if (!m_input)
        {
            Log::Error(LogEngine, "InputSubsystem: Failed to create Input");
            return MakeError("InputSubsystem: Failed to create Input");
        }

        Log::Info(LogEngine, "InputSubsystem: Initialised");
        return {};
    }

    void InputSubsystem::Shutdown()
    {
        if (!m_input)
        {
            return;
        }

        Log::Info(LogEngine, "InputSubsystem: Shutting down");
        m_input.reset();
    }

    auto InputSubsystem::GetInput() -> Input&
    {
        assert(m_input && "GetInput called before Initialise or after Shutdown");
        return *m_input;
    }

} // namespace Wayfinder
