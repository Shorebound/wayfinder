#pragma once

#include "raylib.h"
#include <memory>
#include <string>

namespace Wayfinder
{

    class Game;
    class Renderer;

    class WAYFINDER_API Application
    {
    public:
        struct Config
        {
            int screenWidth = 800;
            int screenHeight = 450;
            std::string windowTitle = "Wayfinder Engine";
        };

        struct CommandLineArgs
        {
            int Count = 0;
            char** Args = nullptr;

            const char* operator[](int index) const
            {
                // WAYFINDER_ASSERT(index < Count);
                return Args[index];
            }
        };

        Application(const Config& config = {});
        ~Application();

        bool Initialize();

        void Run();

        void Shutdown();

    private:
        void Loop();
        static void Loop(Application* app);

        Config m_config;
        bool m_isRunning;

        std::unique_ptr<Game> m_game;
        std::unique_ptr<Renderer> m_renderer;

        double m_lastFrameTime;
    };

    // To be defined by Client
    Application* CreateApplication(const Application::CommandLineArgs& args = {});
} // namespace Wayfinder
