#include "WindowSubsystem.h"

#include "app/ConfigService.h"
#include "app/EngineConfig.h"
#include "app/EngineContext.h"
#include "core/Log.h"
#include "platform/Window.h"

#include <cassert>

namespace Wayfinder
{
    WindowSubsystem::~WindowSubsystem()
    {
        Shutdown();
    }

    auto WindowSubsystem::Initialise(EngineContext& context) -> Result<void>
    {
        Log::Info(LogEngine, "WindowSubsystem: Initialising");

        const auto& configService = context.GetAppSubsystem<ConfigService>();
        const auto& engineConfig = configService.Get<EngineConfig>();

        const auto windowConfig = Window::Config{
            .Width = engineConfig.Window.Width,
            .Height = engineConfig.Window.Height,
            .Title = engineConfig.Window.Title,
            .VSync = engineConfig.Window.VSync,
        };

        m_window = Window::Create(windowConfig, engineConfig.Backends.Platform);
        if (!m_window)
        {
            Log::Error(LogEngine, "WindowSubsystem: Failed to create Window");
            return MakeError("WindowSubsystem: Failed to create Window");
        }

        if (auto result = m_window->Initialise(); !result)
        {
            Log::Error(LogEngine, "WindowSubsystem: Failed to initialise Window -- {}", result.error().GetMessage());
            m_window.reset();
            return std::unexpected(result.error());
        }

        Log::Info(LogEngine, "WindowSubsystem: Initialised");
        return {};
    }

    void WindowSubsystem::Shutdown()
    {
        if (!m_window)
        {
            return;
        }

        Log::Info(LogEngine, "WindowSubsystem: Shutting down");
        m_window->Shutdown();
        m_window.reset();
    }

    auto WindowSubsystem::GetWindow() -> Window&
    {
        assert(m_window && "GetWindow called before Initialise or after Shutdown");
        return *m_window;
    }

    auto WindowSubsystem::ShouldClose() const -> bool
    {
        return m_window && m_window->ShouldClose();
    }

} // namespace Wayfinder
