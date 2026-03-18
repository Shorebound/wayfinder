#include "SDL3Window.h"

#include <SDL3/SDL.h>

namespace Wayfinder
{
    std::unique_ptr<Window> Window::Create(const Window::Config& config, PlatformBackend backend)
    {
        switch (backend)
        {
        case PlatformBackend::SDL3:
            return std::make_unique<SDL3Window>(config);
        }

        return nullptr;
    }

    SDL3Window::SDL3Window(const Window::Config& config)
    {
        m_data = {
            config.Width,
            config.Height,
            config.Title,
            config.VSync
        };
    }

    SDL3Window::~SDL3Window()
    {
        if (m_initialized)
        {
            Shutdown();
        }
    }

    bool SDL3Window::Initialize()
    {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        {
            return false;
        }

        m_window = SDL_CreateWindow(
            m_data.Title.c_str(),
            static_cast<int>(m_data.Width),
            static_cast<int>(m_data.Height),
            0);

        if (!m_window)
        {
            SDL_Quit();
            return false;
        }

        m_initialized = true;
        return true;
    }

    void SDL3Window::Shutdown()
    {
        if (m_window)
        {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }

        SDL_Quit();
        m_initialized = false;
    }

    void SDL3Window::Update()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                m_shouldClose = true;
            }
            else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                m_shouldClose = true;
            }
        }
    }

    bool SDL3Window::ShouldClose() const
    {
        return m_shouldClose;
    }

    void SDL3Window::SetVSync(bool enabled)
    {
        m_data.VSync = enabled;
    }

    void SDL3Window::SetTitle(const std::string& title)
    {
        m_data.Title = title;
        if (m_window)
        {
            SDL_SetWindowTitle(m_window, title.c_str());
        }
    }

    void SDL3Window::SetSize(uint32_t width, uint32_t height)
    {
        m_data.Width = width;
        m_data.Height = height;
        if (m_window)
        {
            SDL_SetWindowSize(m_window, static_cast<int>(width), static_cast<int>(height));
        }
    }
}
