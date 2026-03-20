#pragma once

#include "core/events/Event.h"
#include <format>
#include <memory>

namespace Wayfinder
{

    class WindowResizeEvent : public EventImpl<Event, EventType::WindowResize, EventCategory::Application>
    {
    public:
        WindowResizeEvent(const uint32_t width, const uint32_t height) : m_width(width), m_height(height) {}

        uint32_t GetWidth() const { return m_width; }
        uint32_t GetHeight() const { return m_height; }

        std::string ToString() const override
        {
            return std::format("{}: {}, {}", GetName(), m_width, m_height);
        }

        std::unique_ptr<Event> Clone() const override
        {
            return std::make_unique<WindowResizeEvent>(*this);
        }

    private:
        uint32_t m_width;
        uint32_t m_height;
    };

    class WindowCloseEvent : public EventImpl<Event, EventType::WindowClose, EventCategory::Application>
    {
    public:
        WindowCloseEvent() = default;

        std::unique_ptr<Event> Clone() const override
        {
            return std::make_unique<WindowCloseEvent>(*this);
        }
    };

    class AppTickEvent : public EventImpl<Event, EventType::AppTick, EventCategory::Application>
    {
    public:
        AppTickEvent() = default;

        std::unique_ptr<Event> Clone() const override
        {
            return std::make_unique<AppTickEvent>(*this);
        }
    };

    class AppUpdateEvent : public EventImpl<Event, EventType::AppUpdate, EventCategory::Application>
    {
    public:
        AppUpdateEvent() = default;

        std::unique_ptr<Event> Clone() const override
        {
            return std::make_unique<AppUpdateEvent>(*this);
        }
    };

    class AppRenderEvent : public EventImpl<Event, EventType::AppRender, EventCategory::Application>
    {
    public:
        AppRenderEvent() = default;

        std::unique_ptr<Event> Clone() const override
        {
            return std::make_unique<AppRenderEvent>(*this);
        }
    };
}