#pragma once

#include "core/MouseCodes.h"
#include "events/Event.h"
#include <sstream>

namespace Wayfinder
{

    class MouseMovedEvent : public EventImpl<Event, MouseMovedEvent, EventType::MouseMoved, EventCategory::Mouse | EventCategory::Input>
    {
    public:
        MouseMovedEvent(const float x, const float y) : m_mouseX(x), m_mouseY(y) {}

        float GetX() const { return m_mouseX; }
        float GetY() const { return m_mouseY; }

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << m_mouseX << ", " << m_mouseY;
            return ss.str();
        }

    private:
        float m_mouseX, m_mouseY;
    };

    class MouseScrolledEvent : public EventImpl<Event, MouseScrolledEvent, EventType::MouseScrolled, EventCategory::Mouse | EventCategory::Input>
    {
    public:
        MouseScrolledEvent(const float xOffset, const float yOffset) : m_xOffset(xOffset), m_yOffset(yOffset) {}

        float GetXOffset() const { return m_xOffset; }
        float GetYOffset() const { return m_yOffset; }

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << GetXOffset() << ", " << GetYOffset();
            return ss.str();
        }

    private:
        float m_xOffset, m_yOffset;
    };

    class MouseButtonEvent : public Event
    {
    public:
        MouseCode GetMouseButton() const { return m_button; }

    protected:
        MouseButtonEvent(const MouseCode button) : m_button(button) {}

        MouseCode m_button;
    };

    class MouseButtonPressedEvent : public EventImpl<MouseButtonEvent, MouseButtonPressedEvent, EventType::MouseButtonPressed, EventCategory::MouseButton | EventCategory::Input>
    {
    public:
        MouseButtonPressedEvent(const MouseCode button) : EventImpl(button) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << m_button;
            return ss.str();
        }
    };

    class MouseButtonReleasedEvent : public EventImpl<MouseButtonEvent, MouseButtonReleasedEvent, EventType::MouseButtonReleased, EventCategory::MouseButton | EventCategory::Input>
    {
    public:
        MouseButtonReleasedEvent(const MouseCode button) : EventImpl(button) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << m_button;
            return ss.str();
        }
    };

}