#pragma once

#include "../core/KeyCodes.h"
#include "events/Event.h"
#include <sstream>

namespace Wayfinder
{

    class KeyEvent : public Event
    {
    public:
        KeyCode GetKeyCode() const { return m_KeyCode; }

    protected:
        KeyEvent(const KeyCode keycode) : m_KeyCode(keycode) {}

        KeyCode m_KeyCode;
    };

    class KeyPressedEvent : public EventImpl<KeyEvent, KeyPressedEvent, EventType::KeyPressed, EventCategory::Keyboard | EventCategory::Input>
    {
    public:
        KeyPressedEvent(const KeyCode keycode, bool isRepeat = false) : EventImpl(keycode), m_IsRepeat(isRepeat) {}

        bool IsRepeat() const { return m_IsRepeat; }

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << m_KeyCode << " (repeat = " << m_IsRepeat << ")";
            return ss.str();
        }

    private:
        bool m_IsRepeat;
    };

    class KeyReleasedEvent : public EventImpl<KeyEvent, KeyReleasedEvent, EventType::KeyReleased, EventCategory::Keyboard | EventCategory::Input>
    {
    public:
        KeyReleasedEvent(const KeyCode keycode) : EventImpl(keycode) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << m_KeyCode;
            return ss.str();
        }
    };

    class KeyTypedEvent : public EventImpl<KeyEvent, KeyTypedEvent, EventType::KeyTyped, EventCategory::Keyboard | EventCategory::Input>
    {
    public:
        KeyTypedEvent(const KeyCode keycode) : EventImpl(keycode) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << m_KeyCode;
            return ss.str();
        }
    };
}