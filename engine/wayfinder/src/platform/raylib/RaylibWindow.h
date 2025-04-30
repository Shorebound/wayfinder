#pragma once
#include "../Window.h"

namespace Wayfinder
{
    class RaylibWindow : public Window
    {
    public:

        struct Data
        {
            uint32_t Width;
            uint32_t Height;
            std::string Title;
            bool VSync;
            // EventCallbackFn EventCallback;
        };


        RaylibWindow(const Window::Config& config);
        virtual ~RaylibWindow();

        virtual bool Initialize() override;
        virtual void Shutdown() override;
        virtual void Update() override;

        inline virtual uint32_t GetWidth() const override { return m_data.Width; }
        inline virtual uint32_t GetHeight() const override { return m_data.Height; }
        inline virtual std::string GetTitle() const override { return m_data.Title; }
        inline virtual bool IsFullscreen() const override { return false; }
        inline virtual bool IsVSync() const override { return m_data.VSync; }

        virtual void SetVSync(bool enabled) override;
        virtual void SetTitle(const std::string& title) override;
        virtual void SetSize(uint32_t width, uint32_t height) override;

        virtual bool ShouldClose() const override;

    private:
        RaylibWindow::Data m_data;
        bool m_initialized = false;
    };
}
