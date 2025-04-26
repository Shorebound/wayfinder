#pragma once

#include "raylib.h"

namespace Wayfinder {

class Application {
public:
    Application();
    ~Application();

    // Initialize the application
    bool Initialize(int width, int height, const char* title);

    // Run the application (main loop)
    void Run();

    // Clean up resources
    void Shutdown();

private:
    // Update and render a single frame
    void UpdateFrame();
    void RenderFrame();

    int m_screenWidth;
    int m_screenHeight;
    const char* m_windowTitle;
    bool m_isRunning;
};

} // namespace Wayfinder
