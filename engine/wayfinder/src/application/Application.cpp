#include "Application.h"

#include "core/EngineConfig.h"
#include "core/EngineContext.h"
#include "core/Game.h"
#include "core/Module.h"
#include "core/ModuleRegistry.h"
#include "core/LayerStack.h"
#include "core/Log.h"
#include "core/ProjectDescriptor.h"
#include "core/ProjectResolver.h"
#include "core/events/ApplicationEvent.h"
#include "core/events/MouseEvent.h"
#include "platform/Input.h"
#include "platform/Time.h"
#include "platform/Window.h"
#include "rendering/RenderDevice.h"
#include "rendering/Renderer.h"
#include "rendering/SceneRenderExtractor.h"
#include "scene/Scene.h"

namespace Wayfinder
{
    Application::Application(std::unique_ptr<Module> module,
                             const CommandLineArgs& args)
        : m_module(std::move(module))
    {
    }

    Application::~Application()
    {
        Shutdown();
    }

    void Application::Run()
    {
        if (!Initialize())
        {
            WAYFINDER_ERROR(LogEngine, "Initialization failed — aborting");
            return;
        }

        Loop();
        Shutdown();
    }

    bool Application::Initialize()
    {
        Log::Init();
        WAYFINDER_INFO(LogEngine, "Initializing Wayfinder Engine");

        // Discover project descriptor from CWD
        const auto projectFile = FindProjectFile();
        if (!projectFile)
        {
            WAYFINDER_ERROR(LogEngine,
                "No project.wayfinder found in current directory or any parent. "
                "Run the engine from within a project directory.");
            return false;
        }

        m_project = std::make_unique<ProjectDescriptor>(
            ProjectDescriptor::LoadFromFile(*projectFile));

        // Load engine config from the project's config directory (falls back to defaults)
        m_config = std::make_unique<EngineConfig>(
            EngineConfig::LoadFromFile(m_project->ResolveEngineConfigPath()));

        // Platform services — owned directly, no ServiceLocator
        m_input = Input::Create(m_config->Backends.Platform);
        m_time = Time::Create(m_config->Backends.Platform);
        m_layerStack = std::make_unique<LayerStack>();

        // Window — must exist before RenderDevice
        const auto windowConfig = Window::Config{
            m_config->Window.Width,
            m_config->Window.Height,
            m_config->Window.Title,
            m_config->Window.VSync};

        m_window = Window::Create(windowConfig, m_config->Backends.Platform);

        if (!m_window->Initialize())
        {
            WAYFINDER_ERROR(LogEngine, "Failed to initialize Window");
            return false;
        }

        // Wire window events → Application::OnEvent
        m_window->SetEventCallback(
            [this](Event& e) { OnEvent(e); });

        // GPU device — needs window handle for swapchain
        m_device = RenderDevice::Create(m_config->Backends.Rendering);

        if (!m_device || !m_device->Initialize(*m_window))
        {
            WAYFINDER_ERROR(LogEngine, "Failed to initialize RenderDevice");
            return false;
        }

        // Create module registry and let the game module declare its systems.
        // This must happen before Game::Initialize so scene creation can
        // replay the registered system factories into every new world.
        if (m_module)
        {
            m_moduleRegistry = std::make_unique<ModuleRegistry>(*m_project, *m_config);
            m_module->Register(*m_moduleRegistry);
        }

        // Build context bundle for systems that need it
        EngineContext ctx{*m_window, *m_input, *m_time, *m_config, *m_project,
                          m_moduleRegistry.get()};

        // Game and renderer
        m_game = std::make_unique<Game>();
        m_renderer = std::make_unique<Renderer>();
        m_sceneRenderExtractor = std::make_unique<SceneRenderExtractor>();

        if (!m_game->Initialize(ctx))
        {
            WAYFINDER_ERROR(LogEngine, "Failed to initialize Game");
            return false;
        }

        m_renderer->SetAssetService(m_game->GetAssetService());

        if (!m_renderer->Initialize(*m_device, *m_config))
        {
            WAYFINDER_ERROR(LogEngine, "Failed to initialize Renderer");
            return false;
        }

        if (m_module)
        {
            m_module->OnStartup();
            m_moduleStarted = true;
        }

        m_running = true;
        return true;
    }

    void Application::Loop()
    {
        while (m_running && !m_window->ShouldClose())
        {
            m_time->Update();
            m_input->BeginFrame();
            m_window->Update(); // polls SDL events → dispatches via OnEvent

            // Propagate frame through layers (bottom → top)
            for (auto& layer : *m_layerStack)
            {
                layer->OnUpdate(m_time->GetDeltaTime());
            }

            if (m_game)
            {
                m_game->Update(m_time->GetDeltaTime());

                if (m_renderer)
                {
                    if (const auto* currentScene = m_game->GetCurrentScene())
                    {
                        m_renderer->Render(m_sceneRenderExtractor->Extract(*currentScene));
                    }
                }
            }

            m_running = m_game->IsRunning();
        }
    }

    void Application::OnEvent(Event& event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowCloseEvent>(
            [this](WindowCloseEvent& e) { return OnWindowClose(e); });
        dispatcher.Dispatch<WindowResizeEvent>(
            [this](WindowResizeEvent& e) { return OnWindowResize(e); });

        // Feed scroll events into input accumulator
        dispatcher.Dispatch<MouseScrolledEvent>(
            [this](MouseScrolledEvent& e)
            {
                m_input->AccumulateScroll(e.GetXOffset(), e.GetYOffset());
                return false;
            });

        // Propagate to layers (top → bottom, overlays first)
        for (auto it = m_layerStack->rbegin(); it != m_layerStack->rend(); ++it)
        {
            if (event.Handled)
                break;
            (*it)->OnEvent(event);
        }
    }

    bool Application::OnWindowClose(WindowCloseEvent& /*e*/)
    {
        m_running = false;
        return true;
    }

    bool Application::OnWindowResize(WindowResizeEvent& e)
    {
        WAYFINDER_INFO(LogEngine, "Window resized to {}x{}", e.GetWidth(), e.GetHeight());
        return false;
    }

    LayerStack& Application::GetLayerStack()
    {
        return *m_layerStack;
    }

    void Application::Shutdown()
    {
        if (!m_config) return; // never initialized

        WAYFINDER_INFO(LogEngine, "Shutting down Wayfinder Engine");

        if (m_module && m_moduleStarted)
        {
            m_module->OnShutdown();
            m_moduleStarted = false;
        }

        m_moduleRegistry = nullptr;

        if (m_renderer)
        {
            m_renderer->Shutdown();
            m_renderer = nullptr;
        }

        m_sceneRenderExtractor = nullptr;

        if (m_game)
        {
            m_game->Shutdown();
            m_game = nullptr;
        }

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

        m_layerStack = nullptr;
        m_input = nullptr;
        m_time = nullptr;
        m_config = nullptr;
        m_project = nullptr;

        Log::Shutdown();
    }
} // namespace Wayfinder

