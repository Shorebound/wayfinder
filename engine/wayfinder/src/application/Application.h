#pragma once

#include "core/events/EventQueue.h"

#include <memory>
#include <string>

namespace Wayfinder
{
    class EngineRuntime;
    class Game;
    class Module;
    class ModuleRegistry;
    class LayerStack;
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

        explicit Application(std::unique_ptr<Module> module,
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

        std::unique_ptr<Module> m_module;
        std::unique_ptr<ModuleRegistry> m_moduleRegistry;
        std::unique_ptr<ProjectDescriptor> m_project;
        std::unique_ptr<EngineConfig> m_config;
        bool m_running = false;
        bool m_moduleStarted = false;

        std::unique_ptr<EngineRuntime> m_runtime;
        std::unique_ptr<LayerStack> m_layerStack;
        std::unique_ptr<Game> m_game;
        EventQueue m_eventQueue;
    };

} // namespace Wayfinder
