#include "RenderDeviceSubsystem.h"

#include "app/ConfigService.h"
#include "app/EngineConfig.h"
#include "app/EngineContext.h"
#include "app/WindowSubsystem.h"
#include "core/Log.h"
#include "rendering/backend/RenderDevice.h"

#include <cassert>

namespace Wayfinder
{
    RenderDeviceSubsystem::~RenderDeviceSubsystem()
    {
        Shutdown();
    }

    auto RenderDeviceSubsystem::Initialise(EngineContext& context) -> Result<void>
    {
        Log::Info(LogEngine, "RenderDeviceSubsystem: Initialising");

        const auto& configService = context.GetAppSubsystem<ConfigService>();
        const auto& engineConfig = configService.Get<EngineConfig>();

        m_device = RenderDevice::Create(engineConfig.Backends.Rendering);
        if (!m_device)
        {
            Log::Error(LogEngine, "RenderDeviceSubsystem: Failed to create RenderDevice");
            return MakeError("RenderDeviceSubsystem: Failed to create RenderDevice");
        }

        auto& windowSubsystem = context.GetAppSubsystem<WindowSubsystem>();
        if (auto result = m_device->Initialise(windowSubsystem.GetWindow()); !result)
        {
            Log::Error(LogEngine, "RenderDeviceSubsystem: Failed to initialise RenderDevice -- {}", result.error().GetMessage());
            m_device.reset();
            return std::unexpected(result.error());
        }

        Log::Info(LogEngine, "RenderDeviceSubsystem: Initialised");
        return {};
    }

    void RenderDeviceSubsystem::Shutdown()
    {
        if (!m_device)
        {
            return;
        }

        Log::Info(LogEngine, "RenderDeviceSubsystem: Shutting down");
        m_device->Shutdown();
        m_device.reset();
    }

    auto RenderDeviceSubsystem::GetDevice() -> RenderDevice&
    {
        assert(m_device && "GetDevice called before Initialise or after Shutdown");
        return *m_device;
    }

} // namespace Wayfinder
