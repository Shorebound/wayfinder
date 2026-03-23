#pragma once

#include "core/Result.h"
#include "core/events/Event.h"
#include "platform/BackendConfig.h"

#include <functional>

namespace Wayfinder
{
    class WAYFINDER_API Window
    {
    public:
        using EventCallbackFn = std::function<void(Event&)>;

        struct Config
        {
            uint32_t Width = 1920;
            uint32_t Height = 1080;
            std::string Title = "Wayfinder Engine";
            bool VSync = false;
        };

        virtual ~Window() = default;

        virtual Result<void> Initialise() = 0;
        virtual void Shutdown() = 0;
        virtual void Update() = 0;

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;
        virtual bool IsVSync() const = 0;
        virtual std::string GetTitle() const = 0;
        virtual bool IsFullscreen() const = 0;

        virtual bool ShouldClose() const = 0;
        virtual void* GetNativeHandle() const = 0;

        virtual void SetVSync(bool enabled) = 0;
        virtual void SetTitle(const std::string& title) = 0;
        virtual void SetSize(uint32_t width, uint32_t height) = 0;

        virtual void SetEventCallback(const EventCallbackFn& callback) = 0;

        /**
         * @brief Create a Window with the given configuration.
         * @param config   Window dimensions, title, and VSync settings.
         * @param backend  Platform backend to use (defaults to PlatformBackend::SDL3).
         * @return A std::unique_ptr<Window> owning the platform window.
         */
        static std::unique_ptr<Window> Create(const Window::Config& config, PlatformBackend backend = PlatformBackend::SDL3);

        /**
         * @brief Create a Window with default configuration.
         *
         * Convenience overload that delegates to Create(Config{}).
         * @return A std::unique_ptr<Window> owning the platform window.
         */
        static std::unique_ptr<Window> Create()
        {
            return Create(Config{});
        }
    };

}
