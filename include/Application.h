#pragma once

#include "raylib.h"
#include <memory>
#include <string>

namespace Wayfinder {

// Forward declarations
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

    Application(const Config& config = {});
    ~Application();

    // Initialize the application
    bool Initialize();

    // Run the application (main loop)
    void Run();

    // Clean up resources
    void Shutdown();

private:

    void Loop();
    static void Loop(Application* app);
    // Member variables
    Config m_config;
    bool m_isRunning;

    // Core components
    std::unique_ptr<Game> m_game;
    std::unique_ptr<Renderer> m_renderer;

    // Timing variables
    double m_lastFrameTime;
};

} // namespace Wayfinder
