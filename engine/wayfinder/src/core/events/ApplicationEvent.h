#pragma once

#include "Event.h"
#include <format>

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

    private:
        uint32_t m_width;
        uint32_t m_height;
    };

    class WindowCloseEvent : public EventImpl<Event, EventType::WindowClose, EventCategory::Application>
    {
    public:
        WindowCloseEvent() = default;
    };

    class AppTickEvent : public EventImpl<Event, EventType::AppTick, EventCategory::Application>
    {
    public:
        AppTickEvent() = default;
    };

    class AppUpdateEvent : public EventImpl<Event, EventType::AppUpdate, EventCategory::Application>
    {
    public:
        AppUpdateEvent() = default;
    };

    class AppRenderEvent : public EventImpl<Event, EventType::AppRender, EventCategory::Application>
    {
    public:
        AppRenderEvent() = default;
    };
}