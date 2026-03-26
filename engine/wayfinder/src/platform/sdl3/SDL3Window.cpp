#include "SDL3Window.h"
#include "platform/null/NullWindow.h"

#include "core/events/ApplicationEvent.h"
#include "core/events/KeyEvent.h"
#include "core/events/MouseEvent.h"

#include <SDL3/SDL.h>
#include <format>

namespace Wayfinder
{
    std::unique_ptr<Window> Window::Create(const Window::Config& config, PlatformBackend backend)
    {
        switch (backend)
        {
        case PlatformBackend::SDL3:
            return std::make_unique<SDL3Window>(config);
        case PlatformBackend::Null:
            return std::make_unique<NullWindow>(config);
        }

        return nullptr;
    }

    SDL3Window::SDL3Window(const Window::Config& config) : m_width(config.Width), m_height(config.Height), m_title(config.Title), m_vsync(config.VSync) {}

    SDL3Window::~SDL3Window()
    {
        ReleaseResources();
    }

    Result<void> SDL3Window::Initialise()
    {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        {
            return MakeError(std::format("SDL_Init failed: {}", SDL_GetError()));
        }

        m_window = SDL_CreateWindow(m_title.c_str(), static_cast<int>(m_width), static_cast<int>(m_height), 0);

        if (!m_window)
        {
            auto error = MakeError(std::format("SDL_CreateWindow failed: {}", SDL_GetError()));
            SDL_Quit();
            return error;
        }

        m_initialised = true;
        return {};
    }

    void SDL3Window::Shutdown()
    {
        ReleaseResources();
    }

    void SDL3Window::ReleaseResources()
    {
        if (m_window)
        {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }

        if (m_initialised)
        {
            SDL_Quit();
        }

        m_initialised = false;
    }

    void SDL3Window::Update()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
            {
                m_shouldClose = true;
                if (m_eventCallback)
                {
                    WindowCloseEvent e;
                    m_eventCallback(e);
                }
                break;
            }
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            {
                m_shouldClose = true;
                if (m_eventCallback)
                {
                    WindowCloseEvent e;
                    m_eventCallback(e);
                }
                break;
            }
            case SDL_EVENT_WINDOW_RESIZED:
            {
                m_width = static_cast<uint32_t>(event.window.data1);
                m_height = static_cast<uint32_t>(event.window.data2);
                if (m_eventCallback)
                {
                    WindowResizeEvent e(m_width, m_height);
                    m_eventCallback(e);
                }
                break;
            }
            case SDL_EVENT_KEY_DOWN:
            {
                if (m_eventCallback)
                {
                    const auto key = static_cast<KeyCode>(event.key.scancode);
                    KeyPressedEvent e(key, event.key.repeat);
                    m_eventCallback(e);
                }
                break;
            }
            case SDL_EVENT_KEY_UP:
            {
                if (m_eventCallback)
                {
                    const auto key = static_cast<KeyCode>(event.key.scancode);
                    KeyReleasedEvent e(key);
                    m_eventCallback(e);
                }
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            {
                if (m_eventCallback)
                {
                    const auto button = static_cast<MouseCode>(event.button.button);
                    MouseButtonPressedEvent e(button);
                    m_eventCallback(e);
                }
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_UP:
            {
                if (m_eventCallback)
                {
                    const auto button = static_cast<MouseCode>(event.button.button);
                    MouseButtonReleasedEvent e(button);
                    m_eventCallback(e);
                }
                break;
            }
            case SDL_EVENT_MOUSE_MOTION:
            {
                if (m_eventCallback)
                {
                    MouseMovedEvent e(event.motion.x, event.motion.y);
                    m_eventCallback(e);
                }
                break;
            }
            case SDL_EVENT_MOUSE_WHEEL:
            {
                if (m_eventCallback)
                {
                    MouseScrolledEvent e(event.wheel.x, event.wheel.y);
                    m_eventCallback(e);
                }
                break;
            }
            default:
                break;
            }
        }
    }

    bool SDL3Window::ShouldClose() const
    {
        return m_shouldClose;
    }

    void SDL3Window::SetEventCallback(const EventCallbackFn& callback)
    {
        m_eventCallback = callback;
    }

    void SDL3Window::SetVSync(bool enabled)
    {
        m_vsync = enabled;
    }

    void SDL3Window::SetTitle(std::string_view title)
    {
        m_title.assign(title.begin(), title.end());
        if (m_window)
        {
            SDL_SetWindowTitle(m_window, m_title.c_str());
        }
    }

    void SDL3Window::SetSize(uint32_t width, uint32_t height)
    {
        m_width = width;
        m_height = height;
        if (m_window)
        {
            SDL_SetWindowSize(m_window, static_cast<int>(width), static_cast<int>(height));
        }
    }
}
