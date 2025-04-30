#pragma once

namespace Wayfinder
{
    class Game;
    class Renderer;
    class Window;

    class WAYFINDER_API Application
    {
    public:
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

        struct Config
        {
            uint32_t ScreenWidth = 800;
            uint32_t ScreenHeight = 450;
            std::string WindowTitle = "Wayfinder Engine";
            bool VSync = false;

            Application::CommandLineArgs CommandLineArgs;
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

        std::unique_ptr<Window> m_window;
        std::unique_ptr<Game> m_game;
        std::unique_ptr<Renderer> m_renderer;
    };

    // To be defined by Client
    Application* CreateApplication(const Application::CommandLineArgs& args = {});
} // namespace Wayfinder
