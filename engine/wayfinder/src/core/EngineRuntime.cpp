#include "EngineRuntime.h"

#include "EngineConfig.h"
#include "EngineContext.h"
#include "Log.h"
#include "ProjectDescriptor.h"
#include "../platform/Input.h"
#include "../platform/Time.h"
#include "../platform/Window.h"
#include "../rendering/RenderDevice.h"
#include "../rendering/Renderer.h"
#include "../rendering/SceneRenderExtractor.h"
#include "../scene/Scene.h"

namespace Wayfinder
{
    EngineRuntime::EngineRuntime(const EngineConfig& config,
                                 const ProjectDescriptor& project)
        : m_config(config)
        , m_project(project)
    {
    }

    EngineRuntime::~EngineRuntime()
    {
        Shutdown();
    }

    // ── Lifecycle ────────────────────────────────────────────

    bool EngineRuntime::Initialize()
    {
        WAYFINDER_INFO(LogEngine, "Initializing EngineRuntime");

        // Platform services
        m_input = Input::Create(m_config.Backends.Platform);
        m_time = Time::Create(m_config.Backends.Platform);

        // Window — must exist before RenderDevice (swapchain needs a surface)
        const auto windowConfig = Window::Config{
            m_config.Window.Width,
            m_config.Window.Height,
            m_config.Window.Title,
            m_config.Window.VSync};

        m_window = Window::Create(windowConfig, m_config.Backends.Platform);

        if (!m_window->Initialize())
        {
            WAYFINDER_ERROR(LogEngine, "EngineRuntime: Failed to initialize Window");
            return false;
        }

        // GPU device — needs window handle for swapchain
        m_device = RenderDevice::Create(m_config.Backends.Rendering);

        if (!m_device || !m_device->Initialize(*m_window))
        {
            WAYFINDER_ERROR(LogEngine, "EngineRuntime: Failed to initialize RenderDevice");
            return false;
        }

        // Rendering
        m_renderer = std::make_unique<Renderer>();
        m_extractor = std::make_unique<SceneRenderExtractor>();

        if (!m_renderer->Initialize(*m_device, m_config))
        {
            WAYFINDER_ERROR(LogEngine, "EngineRuntime: Failed to initialize Renderer");
            return false;
        }

        WAYFINDER_INFO(LogEngine, "EngineRuntime initialized");
        return true;
    }

    void EngineRuntime::Shutdown()
    {
        if (!m_renderer && !m_device && !m_window)
            return; // already shut down or never initialized

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

    Window& EngineRuntime::GetWindow() { return *m_window; }
    Input& EngineRuntime::GetInput() { return *m_input; }
    Time& EngineRuntime::GetTime() { return *m_time; }
    RenderDevice& EngineRuntime::GetDevice() { return *m_device; }
    Renderer& EngineRuntime::GetRenderer() { return *m_renderer; }

    // ── Context bundle ───────────────────────────────────────

    EngineContext EngineRuntime::BuildContext() const
    {
        return EngineContext{*m_window, *m_input, *m_time, m_config, m_project};
    }

} // namespace Wayfinder
