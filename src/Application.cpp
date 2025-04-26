#include "../include/Application.h"

namespace Wayfinder {

Application::Application()
    : m_screenWidth(800)
    , m_screenHeight(450)
    , m_windowTitle("Wayfinder Engine")
    , m_isRunning(false)
{
}

Application::~Application()
{
    Shutdown();
}

bool Application::Initialize(int width, int height, const char* title)
{
    m_screenWidth = width;
    m_screenHeight = height;
    m_windowTitle = title;

    // Initialize window and OpenGL context
    InitWindow(m_screenWidth, m_screenHeight, m_windowTitle);
    
    // Set target FPS
    SetTargetFPS(60);
    
    m_isRunning = true;
    return true;
}

void Application::Run()
{
    // Main game loop
    while (m_isRunning && !WindowShouldClose())
    {
        UpdateFrame();
        RenderFrame();
    }
}

void Application::Shutdown()
{
    if (IsWindowReady())
    {
        CloseWindow();
    }
}

void Application::UpdateFrame()
{
    // Update game logic here
}

void Application::RenderFrame()
{
    BeginDrawing();
    
    ClearBackground(RAYWHITE);
    
    // Draw game elements here
    DrawText("Wayfinder Engine is running!", 190, 200, 20, LIGHTGRAY);
    
    EndDrawing();
}

} // namespace Wayfinder
