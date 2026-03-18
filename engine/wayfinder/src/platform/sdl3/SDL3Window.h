#pragma once
#include "../Window.h"

struct SDL_Window;

namespace Wayfinder
{
    class SDL3Window : public Window
    {
    public:
        explicit SDL3Window(const Window::Config& config);
        ~SDL3Window() override;

        bool Initialize() override;
        void Shutdown() override;
        void Update() override;

        uint32_t GetWidth() const override { return m_width; }
        uint32_t GetHeight() const override { return m_height; }
        std::string GetTitle() const override { return m_title; }
        bool IsFullscreen() const override { return false; }
        bool IsVSync() const override { return m_vsync; }

        void SetVSync(bool enabled) override;
        void SetTitle(const std::string& title) override;
        void SetSize(uint32_t width, uint32_t height) override;
        void SetEventCallback(const EventCallbackFn& callback) override;

        bool ShouldClose() const override;
        void* GetNativeHandle() const override { return m_window; }

        SDL_Window* GetNativeWindow() const { return m_window; }

    private:
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        std::string m_title;
        bool m_vsync = false;

        EventCallbackFn m_eventCallback;
        SDL_Window* m_window = nullptr;
        bool m_shouldClose = false;
        bool m_initialized = false;
    };
}
