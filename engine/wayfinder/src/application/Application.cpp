#include "Application.h"

#include "core/Assert.h"
#include "core/EngineConfig.h"
#include "core/EngineRuntime.h"
#include "core/Game.h"
#include "core/GameContext.h"
#include "core/Module.h"
#include "core/ModuleRegistry.h"
#include "core/LayerStack.h"
#include "core/Log.h"
#include "core/ProjectDescriptor.h"
#include "core/ProjectResolver.h"
#include "core/events/ApplicationEvent.h"
#include "core/events/MouseEvent.h"
#include "platform/Input.h"
#include "platform/Window.h"
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
        if (!Initialise())
        {
            WAYFINDER_ERROR(LogEngine, "Initialisation failed — aborting");
            return;
        }

        Loop();
        Shutdown();
    }

    Result<void> Application::Initialise()
    {
        Log::Init();
        WAYFINDER_INFO(LogEngine, "Initialising Wayfinder Engine");

        // 1. Discover project descriptor from CWD
        const auto projectFile = FindProjectFile();
        if (!projectFile)
        {
            WAYFINDER_ERROR(LogEngine,
                "No project.wayfinder found in current directory or any parent. "
                "Run the engine from within a project directory.");
            Log::Shutdown();
            return MakeError("No project.wayfinder found");
        }

        auto loadResult = ProjectDescriptor::LoadFromFile(*projectFile);

        if (!loadResult)
        {
            WAYFINDER_ERROR(LogEngine, "Failed to load project descriptor");
            Log::Shutdown();
            return MakeError("Failed to load project descriptor");
        }

        for (const auto& warning : loadResult->Warnings)
        {
            WAYFINDER_WARNING(LogEngine, "Project: {}", warning);
        }

        m_project = std::make_unique<ProjectDescriptor>(std::move(loadResult->Descriptor));

        // 2. Load engine config
        m_config = std::make_unique<EngineConfig>(
            EngineConfig::LoadFromFile(m_project->ResolveEngineConfigPath()));

        // 3. Module registration (before Game so scene creation can use factories)
        if (m_module)
        {
            m_moduleRegistry = std::make_unique<ModuleRegistry>(*m_project, *m_config);
            m_module->Register(*m_moduleRegistry);
        }

        // 4. Platform + rendering services
        m_runtime = std::make_unique<EngineRuntime>(*m_config, *m_project);
        if (!m_runtime->Initialise())
        {
            WAYFINDER_ERROR(LogEngine, "Failed to initialise EngineRuntime");
            Shutdown();
            return MakeError("Failed to initialise EngineRuntime");
        }

        // Wire window events → Application::OnEvent
        m_runtime->GetWindow().SetEventCallback(
            [this](Event& e) { OnEvent(e); });

        // 5. Layer stack
        m_layerStack = std::make_unique<LayerStack>();

        // 6. Game
        GameContext gameCtx{*m_project, m_moduleRegistry.get()};
        m_game = std::make_unique<Game>();

        if (!m_game->Initialise(gameCtx))
        {
            WAYFINDER_ERROR(LogEngine, "Failed to initialise Game");
            Shutdown();
            return MakeError("Failed to initialise Game");
        }

        m_runtime->SetAssetService(m_game->GetAssetService());

        // 7. Module startup
        if (m_module)
        {
            m_module->OnStartup();
            m_moduleStarted = true;
        }

        m_running = true;
        return {};
    }

    void Application::Loop()
    {
        while (m_running && !m_runtime->ShouldClose())
        {
            m_runtime->BeginFrame();

            // Drain queued input events — single batch at a well-defined frame point
            m_eventQueue.Drain([this](Event& e)
            {
                PropagateToLayers(e);
            });

            const float dt = m_runtime->GetDeltaTime();

            // Propagate frame through layers (bottom → top)
            for (auto& layer : *m_layerStack)
            {
                layer->OnUpdate(dt);
            }

            if (m_game)
            {
                m_game->Update(dt);

                if (const auto* currentScene = m_game->GetCurrentScene())
                {
                    m_runtime->RenderScene(*currentScene);
                }
            }

            m_runtime->EndFrame();
            m_running = m_game->IsRunning();
        }
    }

    void Application::OnEvent(Event& event)
    {
        // Latency-sensitive events: dispatch immediately
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowCloseEvent>(
            [this](WindowCloseEvent& e) { return OnWindowClose(e); });
        dispatcher.Dispatch<WindowResizeEvent>(
            [this](WindowResizeEvent& e) { return OnWindowResize(e); });

        // Feed scroll events into input accumulator immediately
        dispatcher.Dispatch<MouseScrolledEvent>(
            [this](MouseScrolledEvent& e)
            {
                m_runtime->GetInput().AccumulateScroll(e.GetXOffset(), e.GetYOffset());
                return true;
            });

        if (event.Handled)
            return;

        // Defer input events to the queue for batched dispatch
        if (event.IsInCategory(EventCategory::Input))
        {
            auto queuedEvent = event.Clone();
            WAYFINDER_ASSERT(queuedEvent != nullptr,
                             "Deferred event '{}' must implement Clone()",
                             event.GetName());
            m_eventQueue.Push(std::move(queuedEvent));
            return;
        }

        // Non-input, non-handled events: propagate to layers immediately
        PropagateToLayers(event);
    }

    void Application::PropagateToLayers(Event& event)
    {
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
        if (!m_config) return; // never initialised

        WAYFINDER_INFO(LogEngine, "Shutting down Wayfinder Engine");

        if (m_module && m_moduleStarted)
        {
            m_module->OnShutdown();
            m_moduleStarted = false;
        }

        m_moduleRegistry = nullptr;

        if (m_game)
        {
            m_game->Shutdown();
            m_game = nullptr;
        }

        if (m_runtime)
        {
            m_runtime->Shutdown();
            m_runtime = nullptr;
        }

        m_layerStack = nullptr;
        m_config = nullptr;
        m_project = nullptr;

        Log::Shutdown();
    }
} // namespace Wayfinder
