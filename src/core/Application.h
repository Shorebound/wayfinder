#pragma once

#include "raylib.h"
#include <memory>
#include <string>

namespace Wayfinder
{

    class Game;
    class Renderer;

    class Application
    {
    public:
        struct Config
        {
            int screenWidth = 800;
            int screenHeight = 450;
            std::string windowTitle = "Wayfinder Engine";
        };

        Application(const Config &config = {});
        ~Application();

        bool Initialize();

        void Run();

        void Shutdown();

    private:
        void Loop();
        static void Loop(Application *app);

        Config m_config;
        bool m_isRunning;

        std::unique_ptr<Game> m_game;
        std::unique_ptr<Renderer> m_renderer;

        double m_lastFrameTime;
    };

} // namespace Wayfinder
