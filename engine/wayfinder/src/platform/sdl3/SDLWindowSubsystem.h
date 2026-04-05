#pragma once

#include "app/AppSubsystem.h"
#include "core/Result.h"
#include "core/events/Event.h"
#include "wayfinder_exports.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

struct SDL_Window;

namespace Wayfinder
{
    class EngineContext;

    /**
     * @brief Direct SDL3 window management as an AppSubsystem.
     *
     * Owns the SDL3 window and event polling. Replaces the three-tier
     * Window (abstract) + SDL3Window (impl) + WindowSubsystem (wrapper) with
     * a single concrete type. Requires the Presentation capability.
     */
    class WAYFINDER_API SDLWindowSubsystem final : public AppSubsystem
    {
    public:
        using EventCallbackFn = std::function<void(Event&)>;

        SDLWindowSubsystem() = default;
        ~SDLWindowSubsystem() override;

        [[nodiscard]] auto Initialise(EngineContext& context) -> Result<void> override;
        void Shutdown() override;

        /// Poll SDL events and dispatch through the event callback.
        void PollEvents();

        void SetEventCallback(EventCallbackFn callback);

        [[nodiscard]] auto GetWidth() const -> uint32_t
        {
            return m_width;
        }
        [[nodiscard]] auto GetHeight() const -> uint32_t
        {
            return m_height;
        }
        [[nodiscard]] auto GetTitle() const -> const std::string&
        {
            return m_title;
        }
        [[nodiscard]] auto IsVSync() const -> bool
        {
            return m_vsync;
        }
        [[nodiscard]] auto IsFullscreen() const -> bool
        {
            return false;
        }
        [[nodiscard]] auto ShouldClose() const -> bool
        {
            return m_shouldClose;
        }

        [[nodiscard]] auto GetNativeWindow() const -> SDL_Window*
        {
            return m_window;
        }
        [[nodiscard]] auto GetNativeHandle() const -> void*
        {
            return m_window;
        }

        void SetVSync(bool enabled);
        void SetTitle(std::string_view title);
        void SetSize(uint32_t width, uint32_t height);

    private:
        void ReleaseResources();

        uint32_t m_width = 0;
        uint32_t m_height = 0;
        std::string m_title;
        bool m_vsync = false;

        EventCallbackFn m_eventCallback;
        SDL_Window* m_window = nullptr;
        bool m_shouldClose = false;
        bool m_initialised = false;
    };

} // namespace Wayfinder
