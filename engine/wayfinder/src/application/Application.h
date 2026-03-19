#pragma once

#include <memory>
#include <string>

namespace Wayfinder
{
    class Game;
    class GameModule;
    class Input;
    class LayerStack;
    class RenderDevice;
    class Renderer;
    class SceneRenderExtractor;
    class Time;
    class Window;
    struct EngineConfig;
    struct ProjectDescriptor;

    class WAYFINDER_API Application
    {
    public:
        struct CommandLineArgs
        {
            int Count = 0;
            char** Args = nullptr;

            const char* operator[](int index) const
            {
                return Args[index];
            }
        };

        explicit Application(std::unique_ptr<GameModule> gameModule,
                             const CommandLineArgs& args = {});
        ~Application();

        void Run();

        LayerStack& GetLayerStack();

    private:
        bool Initialize();
        void Loop();
        void Shutdown();

        void OnEvent(class Event& event);
        bool OnWindowClose(class WindowCloseEvent& e);
        bool OnWindowResize(class WindowResizeEvent& e);

        std::unique_ptr<GameModule> m_gameModule;
        std::unique_ptr<ProjectDescriptor> m_project;
        std::unique_ptr<EngineConfig> m_config;
        bool m_running = false;

        std::unique_ptr<Window> m_window;
        std::unique_ptr<Input> m_input;
        std::unique_ptr<Time> m_time;
        std::unique_ptr<LayerStack> m_layerStack;

        std::unique_ptr<RenderDevice> m_device;
        std::unique_ptr<Game> m_game;
        std::unique_ptr<Renderer> m_renderer;
        std::unique_ptr<SceneRenderExtractor> m_sceneRenderExtractor;
    };

} // namespace Wayfinder
