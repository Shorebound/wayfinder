#pragma once

#include "events/Event.h"
#include <sstream>

namespace Wayfinder
{

    class WindowResizeEvent : public EventImpl<Event, WindowResizeEvent, EventType::WindowResize, EventCategory::Application>
    {
    public:
        WindowResizeEvent(unsigned int width, unsigned int height) : m_Width(width), m_height(height) {}

        unsigned int GetWidth() const { return m_Width; }
        unsigned int GetHeight() const { return m_height; }

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << m_Width << ", " << m_height;
            return ss.str();
        }

    private:
        unsigned int m_Width, m_height;
    };

    class WindowCloseEvent : public EventImpl<Event, WindowCloseEvent, EventType::WindowClose, EventCategory::Application>
    {
    public:
        WindowCloseEvent() = default;
    };

    class AppTickEvent : public EventImpl<Event, AppTickEvent, EventType::AppTick, EventCategory::Application>
    {
    public:
        AppTickEvent() = default;
    };

    class AppUpdateEvent : public EventImpl<Event, AppUpdateEvent, EventType::AppUpdate, EventCategory::Application>
    {
    public:
        AppUpdateEvent() = default;
    };

    class AppRenderEvent : public EventImpl<Event, AppRenderEvent, EventType::AppRender, EventCategory::Application>
    {
    public:
        AppRenderEvent() = default;
    };
}