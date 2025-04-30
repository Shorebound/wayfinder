#include "RaylibWindow.h"
#include "raylib.h"

namespace Wayfinder
{
    std::unique_ptr<Window> Window::Create(const Window::Config& config)
    {
        return std::make_unique<RaylibWindow>(config);
    }

    RaylibWindow::RaylibWindow(const Window::Config& config)
    {
        m_data = {
            config.Width,
            config.Height,
            config.Title,
            config.VSync
        };
    }

    RaylibWindow::~RaylibWindow()
    {
        if (m_initialized)
        {
            Shutdown();
        }
    }

    bool RaylibWindow::Initialize()
    {
        InitWindow(m_data.Width, m_data.Height, m_data.Title.c_str());
        SetExitKey(0); 
        
        if (m_data.VSync)
        {
            SetTargetFPS(60);
        }
        
        m_initialized = true;
        return true;
    }

    void RaylibWindow::Shutdown()
    {
        if (IsWindowReady())
        {
            CloseWindow();
        }
        m_initialized = false;
    }

    void RaylibWindow::Update()
    {
        // Process window events
        // Raylib handles this internally
    }

    bool RaylibWindow::ShouldClose() const
    {
        return WindowShouldClose();
    }

    void RaylibWindow::SetVSync(bool enabled)
    {
        m_data.VSync = enabled;
        if (enabled)
        {
            SetTargetFPS(60);
        }
        else
        {
            SetTargetFPS(0);
        }
    }

    void RaylibWindow::SetTitle(const std::string& title)
    {
        m_data.Title = title;
        SetWindowTitle(title.c_str());
    }

    void RaylibWindow::SetSize(uint32_t width, uint32_t height)
    {
        m_data.Width = width;
        m_data.Height = height;
        SetWindowSize(width, height);
    }
}

