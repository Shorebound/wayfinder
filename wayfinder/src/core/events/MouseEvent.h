#pragma once

#include "core/MouseCodes.h"
#include "events/Event.h"
#include <sstream>

namespace Wayfinder
{

    class MouseMovedEvent : public EventImpl<Event, MouseMovedEvent, EventType::MouseMoved, EventCategory::Mouse | EventCategory::Input>
    {
    public:
        MouseMovedEvent(const float x, const float y) : m_MouseX(x), m_MouseY(y) {}

        float GetX() const { return m_MouseX; }
        float GetY() const { return m_MouseY; }

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << m_MouseX << ", " << m_MouseY;
            return ss.str();
        }

    private:
        float m_MouseX, m_MouseY;
    };

    class MouseScrolledEvent : public EventImpl<Event, MouseScrolledEvent, EventType::MouseScrolled, EventCategory::Mouse | EventCategory::Input>
    {
    public:
        MouseScrolledEvent(const float xOffset, const float yOffset)
            : m_XOffset(xOffset), m_YOffset(yOffset) {}

        float GetXOffset() const { return m_XOffset; }
        float GetYOffset() const { return m_YOffset; }

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << GetXOffset() << ", " << GetYOffset();
            return ss.str();
        }

    private:
        float m_XOffset, m_YOffset;
    };

    class MouseButtonEvent : public Event
    {
    public:
        MouseCode GetMouseButton() const { return m_Button; }

    protected:
        MouseButtonEvent(const MouseCode button) : m_Button(button) {}

        MouseCode m_Button;
    };

    class MouseButtonPressedEvent : public EventImpl<MouseButtonEvent, MouseButtonPressedEvent, EventType::MouseButtonPressed, EventCategory::MouseButton | EventCategory::Input>
    {
    public:
        MouseButtonPressedEvent(const MouseCode button) : EventImpl(button) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << m_Button;
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
            ss << GetName() << ": " << m_Button;
            return ss.str();
        }
    };

}