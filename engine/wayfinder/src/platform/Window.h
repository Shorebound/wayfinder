#pragma once

#include "../core/BackendConfig.h"

namespace Wayfinder
{
    class WAYFINDER_API Window
    {
    public:
        struct Config
        {
            uint32_t Width = 1920;
            uint32_t Height = 1080;
            std::string Title = "Wayfinder Engine";
            bool VSync = false;
        };

        // Define event types if using callbacks
        // using EventCallbackFn = std::function<void(Event&)>

        virtual ~Window() = default;

        virtual bool Initialize() = 0;
        virtual void Shutdown() = 0;
        virtual void Update() = 0;

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;
        virtual bool IsVSync() const = 0;
        virtual std::string GetTitle() const = 0;
        virtual bool IsFullscreen() const = 0;
        //virtual void SetFullscreen(bool fullscreen) = 0;
        //virtual void SetBorderless(bool borderless) = 0;

        virtual bool ShouldClose() const = 0;

        virtual void SetVSync(bool enabled) = 0;
        virtual void SetTitle(const std::string& title) = 0;
        virtual void SetSize(uint32_t width, uint32_t height) = 0;

        // Called each frame to process window events (like close button)
        // and potentially input events.
        //virtual void PollEvents() = 0

        // Swaps the front and back buffers to display the rendered image.
        //virtual void SwapBuffers() = 0

        //virtual void* GetNativeWindowHandle() const {}

        // Optional: Set event callback if using an event system
        // virtual void SetEventCallback(const EventCallbackFn& callback) = 0;

        static std::unique_ptr<Window> Create(
            const Window::Config& config = {},
            PlatformBackend backend = PlatformBackend::Raylib);

        
    };

}

