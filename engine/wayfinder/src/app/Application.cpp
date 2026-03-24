#include "Application.h"

#include "EngineConfig.h"
#include "EngineRuntime.h"
#include "LayerStack.h"
#include "core/Assert.h"
#include "core/Log.h"
#include "core/events/ApplicationEvent.h"
#include "core/events/KeyEvent.h"
#include "core/events/MouseEvent.h"
#include "gameplay/Game.h"
#include "gameplay/GameContext.h"
#include "platform/Input.h"
#include "platform/Window.h"
#include "plugins/Plugin.h"
#include "plugins/PluginRegistry.h"
#include "project/ProjectDescriptor.h"
#include "project/ProjectResolver.h"

namespace Wayfinder
{
    Application::Application(std::unique_ptr<Plugins::Plugin> gamePlugin, const CommandLineArgs& args) : m_gamePlugin(std::move(gamePlugin)), m_args(args) {}

    Application::~Application()
    {
        Shutdown();
    }

    void Application::Run()
    {
        auto result = Initialise();
        if (!result)
        {
            WAYFINDER_ERROR(LogEngine, "Initialisation failed: {}", result.error().GetMessage());
            Shutdown();
            return;
        }

        Loop();
        Shutdown();
    }

    Result<void> Application::Initialise()
    {
        Log::Init();
        m_logInitialised = true;
        WAYFINDER_INFO(LogEngine, "Initialising Wayfinder Engine");

        // 1. Discover project descriptor from CWD
        auto projectFile = FindProjectFile();
        if (!projectFile)
        {
            return std::unexpected(projectFile.error());
        }

        auto loadResult = ProjectDescriptor::LoadFromFile(*projectFile);
        if (!loadResult)
        {
            return std::unexpected(loadResult.error());
        }

        for (const auto& warning : loadResult->Warnings)
        {
            WAYFINDER_WARNING(LogEngine, "Project: {}", warning);
        }

        m_project = std::make_unique<ProjectDescriptor>(std::move(loadResult->Descriptor));

        // 2. Load engine config
        m_config = std::make_unique<EngineConfig>(EngineConfig::LoadFromFile(m_project->ResolveEngineConfigPath()));

        // 3. Plugin registration (before Game so scene creation can use factories)
        m_pluginRegistry = std::make_unique<Plugins::PluginRegistry>(*m_project, *m_config);
        if (m_gamePlugin)
        {
            m_pluginRegistry->AddPlugin(std::move(m_gamePlugin));
        }

        // 4. Platform + rendering services
        m_runtime = std::make_unique<EngineRuntime>(*m_config, *m_project);
        if (auto runtimeResult = m_runtime->Initialise(); !runtimeResult)
        {
            return std::unexpected(runtimeResult.error());
        }

        // Wire window events → Application::OnEvent
        m_runtime->GetWindow().SetEventCallback([this](Event& e)
        {
            OnEvent(e);
        });

        // 5. Layer stack
        m_layerStack = std::make_unique<LayerStack>();

        // 6. Game
        WAYFINDER_ASSERT(m_pluginRegistry, "Plugin registry must exist after initialisation");
        const GameContext gameCtx{.project = *m_project, .pluginRegistry = *m_pluginRegistry};
        m_game = std::make_unique<Game>(*m_pluginRegistry);

        if (auto gameResult = m_game->Initialise(gameCtx); !gameResult)
        {
            return std::unexpected(gameResult.error());
        }

        m_runtime->SetAssetService(m_game->GetAssetService());

        // 7. Plugin startup lifecycle
        m_pluginRegistry->NotifyStartup();
        m_pluginsStarted = true;

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
        dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& e)
        {
            return OnWindowClose(e);
        });

        dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& e)
        {
            return OnWindowResize(e);
        });

        // Feed scroll events into input accumulator immediately
        dispatcher.Dispatch<MouseScrolledEvent>([this](MouseScrolledEvent& e)
        {
            m_runtime->GetInput().AccumulateScroll(Input::ScrollDelta{
                .X = e.GetXOffset(),
                .Y = e.GetYOffset(),
            });
            return true;
        });

        if (event.Handled)
        {
            return;
        }

        // Defer input events to typed buffers for batched dispatch
        if (event.IsInCategory(EventCategory::Input))
        {
            DeferInputEvent(event);
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
            {
                break;
            }
            (*it)->OnEvent(event);
        }
    }

    void Application::DeferInputEvent(Event& event)
    {
        switch (event.GetEventType())
        {
        case EventType::KeyPressed:
            m_eventQueue.Push(static_cast<const KeyPressedEvent&>(event));
            break;
        case EventType::KeyReleased:
            m_eventQueue.Push(static_cast<const KeyReleasedEvent&>(event));
            break;
        case EventType::KeyTyped:
            m_eventQueue.Push(static_cast<const KeyTypedEvent&>(event));
            break;
        case EventType::MouseMoved:
            m_eventQueue.Push(static_cast<const MouseMovedEvent&>(event));
            break;
        case EventType::MouseButtonPressed:
            m_eventQueue.Push(static_cast<const MouseButtonPressedEvent&>(event));
            break;
        case EventType::MouseButtonReleased:
            m_eventQueue.Push(static_cast<const MouseButtonReleasedEvent&>(event));
            break;
        default:
            WAYFINDER_ASSERT(false, "Unhandled input event type for deferral: {}", event.GetName());
            break;
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
        if (m_config)
        {
            WAYFINDER_INFO(LogEngine, "Shutting down Wayfinder Engine");

            if (m_pluginRegistry && m_pluginsStarted)
            {
                m_pluginRegistry->NotifyShutdown();
                m_pluginsStarted = false;
            }

            /// Tear down Game while PluginRegistry still exists: Game holds a reference to the
            /// registry and must not run after the unique_ptr has released the object.
            if (m_game)
            {
                m_game->Shutdown();
                m_game = nullptr;
            }

            m_pluginRegistry = nullptr;

            if (m_runtime)
            {
                m_runtime->Shutdown();
                m_runtime = nullptr;
            }

            m_layerStack = nullptr;
            m_config = nullptr;
            m_project = nullptr;
        }

        if (m_logInitialised)
        {
            Log::Shutdown();
            m_logInitialised = false;
        }
    }
} // namespace Wayfinder
