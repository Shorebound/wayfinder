#include "EngineRuntime.h"

#include "EngineConfig.h"
#include "EngineContext.h"
#include "core/Log.h"
#include "core/Result.h"
#include "platform/Input.h"
#include "platform/Time.h"
#include "platform/Window.h"
#include "project/ProjectDescriptor.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/pipeline/Renderer.h"
#include "rendering/pipeline/SceneRenderExtractor.h"
#include "scene/Scene.h"

#include <cassert>

namespace Wayfinder
{
    EngineRuntime::EngineRuntime(const EngineConfig& config, const ProjectDescriptor& project) : m_config(config), m_project(project) {}

    EngineRuntime::~EngineRuntime()
    {
        Shutdown();
    }

    // ── Lifecycle ────────────────────────────────────────────

    Result<void> EngineRuntime::Initialise()
    {
        WAYFINDER_INFO(LogEngine, "Initialising EngineRuntime");

        // Platform services
        m_input = Input::Create(m_config.Backends.Platform);
        if (!m_input)
        {
            WAYFINDER_ERROR(LogEngine, "EngineRuntime: Failed to create Input");
            return MakeError("Failed to create Input");
        }

        m_time = Time::Create(m_config.Backends.Platform);
        if (!m_time)
        {
            WAYFINDER_ERROR(LogEngine, "EngineRuntime: Failed to create Time");
            m_input = nullptr;
            return MakeError("Failed to create Time");
        }

        // Window — must exist before RenderDevice (swapchain needs a surface)
        const auto windowConfig = Window::Config{m_config.Window.Width, m_config.Window.Height, m_config.Window.Title, m_config.Window.VSync};

        m_window = Window::Create(windowConfig, m_config.Backends.Platform);
        if (!m_window)
        {
            WAYFINDER_ERROR(LogEngine, "EngineRuntime: Failed to create Window");
            m_time = nullptr;
            m_input = nullptr;
            return MakeError("Failed to create Window");
        }

        if (!m_window->Initialise())
        {
            WAYFINDER_ERROR(LogEngine, "EngineRuntime: Failed to initialise Window");
            m_window = nullptr;
            m_time = nullptr;
            m_input = nullptr;
            return MakeError("Failed to initialise Window");
        }

        // GPU device — needs window handle for swapchain
        m_device = RenderDevice::Create(m_config.Backends.Rendering);

        if (!m_device || !m_device->Initialise(*m_window))
        {
            WAYFINDER_ERROR(LogEngine, "EngineRuntime: Failed to initialise RenderDevice");
            m_device = nullptr;
            m_window->Shutdown();
            m_window = nullptr;
            m_time = nullptr;
            m_input = nullptr;
            return MakeError("Failed to initialise RenderDevice");
        }

        // Rendering
        m_renderer = std::make_unique<Renderer>();
        m_extractor = std::make_unique<SceneRenderExtractor>();

        if (!m_renderer->Initialise(*m_device, m_config))
        {
            WAYFINDER_ERROR(LogEngine, "EngineRuntime: Failed to initialise Renderer");
            m_renderer = nullptr;
            m_extractor = nullptr;
            m_device->Shutdown();
            m_device = nullptr;
            m_window->Shutdown();
            m_window = nullptr;
            m_time = nullptr;
            m_input = nullptr;
            return MakeError("Failed to initialise Renderer");
        }

        WAYFINDER_INFO(LogEngine, "EngineRuntime initialised");
        return {};
    }

    void EngineRuntime::Shutdown()
    {
        if (!m_renderer && !m_device && !m_window && !m_input && !m_time) return; // already shut down or never initialised

        WAYFINDER_INFO(LogEngine, "Shutting down EngineRuntime");

        if (m_renderer)
        {
            m_renderer->Shutdown();
            m_renderer = nullptr;
        }

        m_extractor = nullptr;

        if (m_device)
        {
            m_device->Shutdown();
            m_device = nullptr;
        }

        if (m_window)
        {
            m_window->Shutdown();
            m_window = nullptr;
        }

        m_input = nullptr;
        m_time = nullptr;
    }

    // ── Per-frame ────────────────────────────────────────────

    void EngineRuntime::BeginFrame()
    {
        m_time->Update();
        m_input->BeginFrame();
        m_window->Update();
    }

    void EngineRuntime::EndFrame()
    {
        // Placeholder — future GPU present/swap synchronisation goes here.
    }

    // ── Rendering convenience ────────────────────────────────

    void EngineRuntime::RenderScene(const Scene& scene)
    {
        if (m_renderer && m_extractor)
        {
            m_renderer->Render(m_extractor->Extract(scene));
        }
    }

    void EngineRuntime::SetAssetService(const std::shared_ptr<AssetService>& assetService)
    {
        if (m_renderer)
        {
            m_renderer->SetAssetService(assetService);
        }
    }

    // ── Queries ──────────────────────────────────────────────

    bool EngineRuntime::ShouldClose() const
    {
        return m_window && m_window->ShouldClose();
    }

    float EngineRuntime::GetDeltaTime() const
    {
        return m_time ? m_time->GetDeltaTime() : 0.0f;
    }

    // ── Non-owning accessors ─────────────────────────────────

    Window& EngineRuntime::GetWindow()
    {
        assert(m_window && "GetWindow called before Initialise or after Shutdown");
        return *m_window;
    }
    Input& EngineRuntime::GetInput()
    {
        assert(m_input && "GetInput called before Initialise or after Shutdown");
        return *m_input;
    }
    Time& EngineRuntime::GetTime()
    {
        assert(m_time && "GetTime called before Initialise or after Shutdown");
        return *m_time;
    }
    RenderDevice& EngineRuntime::GetDevice()
    {
        assert(m_device && "GetDevice called before Initialise or after Shutdown");
        return *m_device;
    }
    Renderer& EngineRuntime::GetRenderer()
    {
        assert(m_renderer && "GetRenderer called before Initialise or after Shutdown");
        return *m_renderer;
    }

    // ── Context bundle ───────────────────────────────────────

    EngineContext EngineRuntime::BuildContext() const
    {
        return EngineContext{*m_window, *m_input, *m_time, m_config, m_project};
    }

} // namespace Wayfinder
