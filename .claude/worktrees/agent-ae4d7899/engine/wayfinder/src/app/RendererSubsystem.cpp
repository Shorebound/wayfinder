#include "RendererSubsystem.h"

#include "app/ConfigService.h"
#include "app/EngineConfig.h"
#include "app/EngineContext.h"
#include "app/RenderDeviceSubsystem.h"
#include "core/Log.h"
#include "rendering/pipeline/Renderer.h"

#include <cassert>

namespace Wayfinder
{
    RendererSubsystem::~RendererSubsystem()
    {
        Shutdown();
    }

    auto RendererSubsystem::Initialise(EngineContext& context) -> Result<void>
    {
        Log::Info(LogEngine, "RendererSubsystem: Initialising");

        const auto& configService = context.GetAppSubsystem<ConfigService>();
        const auto& engineConfig = configService.Get<EngineConfig>();

        m_renderer = std::make_unique<Renderer>();

        auto& deviceSubsystem = context.GetAppSubsystem<RenderDeviceSubsystem>();
        if (auto result = m_renderer->Initialise(deviceSubsystem.GetDevice(), engineConfig, &m_blendableEffectRegistry); !result)
        {
            Log::Error(LogEngine, "RendererSubsystem: Failed to initialise Renderer -- {}", result.error().GetMessage());
            m_renderer.reset();
            return std::unexpected(result.error());
        }

        m_canvases.Reset();

        Log::Info(LogEngine, "RendererSubsystem: Initialised");
        return {};
    }

    void RendererSubsystem::Shutdown()
    {
        if (!m_renderer)
        {
            return;
        }

        Log::Info(LogEngine, "RendererSubsystem: Shutting down");
        m_renderer->Shutdown();
        m_renderer.reset();
    }

    auto RendererSubsystem::GetRenderer() -> Renderer&
    {
        assert(m_renderer && "GetRenderer called before Initialise or after Shutdown");
        return *m_renderer;
    }

    auto RendererSubsystem::GetCanvases() -> FrameCanvases&
    {
        return m_canvases;
    }

    auto RendererSubsystem::GetCanvases() const -> const FrameCanvases&
    {
        return m_canvases;
    }

    auto RendererSubsystem::GetBlendableEffectRegistry() -> BlendableEffectRegistry&
    {
        return m_blendableEffectRegistry;
    }

    auto RendererSubsystem::GetBlendableEffectRegistry() const -> const BlendableEffectRegistry&
    {
        return m_blendableEffectRegistry;
    }

} // namespace Wayfinder
