#pragma once

#include "platform/Window.h"

#include <string>

namespace Wayfinder
{
    class NullWindow final : public Window
    {
    public:
        explicit NullWindow(const Config& config = {}) : m_width(config.Width), m_height(config.Height), m_title(config.Title), m_vsync(config.VSync) {}

        Result<void> Initialise() override
        {
            return {};
        }
        void Shutdown() override {}
        void Update() override {}

        uint32_t GetWidth() const override
        {
            return m_width;
        }
        uint32_t GetHeight() const override
        {
            return m_height;
        }
        bool IsVSync() const override
        {
            return m_vsync;
        }
        std::string GetTitle() const override
        {
            return m_title;
        }
        bool IsFullscreen() const override
        {
            return false;
        }

        bool ShouldClose() const override
        {
            return m_shouldClose;
        }
        void* GetNativeHandle() const override
        {
            return nullptr;
        }

        void SetVSync(bool enabled) override
        {
            m_vsync = enabled;
        }
        void SetTitle(std::string_view title) override
        {
            m_title.assign(title.begin(), title.end());
        }
        void SetSize(uint32_t width, uint32_t height) override
        {
            m_width = width;
            m_height = height;
        }

        void SetEventCallback(const EventCallbackFn&) override {}

        /// Test-only: force ShouldClose() to return true.
        void RequestClose()
        {
            m_shouldClose = true;
        }

    private:
        uint32_t m_width;
        uint32_t m_height;
        std::string m_title;
        bool m_vsync;
        bool m_shouldClose = false;
    };

} // namespace Wayfinder
