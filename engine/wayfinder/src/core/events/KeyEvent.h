#pragma once

#include "core/KeyCodes.h"
#include "core/events/Event.h"
#include <sstream>

namespace Wayfinder
{

    class KeyEvent : public Event
    {
    public:
        KeyCode GetKeyCode() const { return m_keyCode; }

    protected:
        KeyEvent(const KeyCode keycode) : m_keyCode(keycode) {}

        KeyCode m_keyCode;
    };

    class KeyPressedEvent : public EventImpl<KeyEvent, KeyPressedEvent, EventType::KeyPressed, EventCategory::Keyboard | EventCategory::Input>
    {
    public:
        KeyPressedEvent(const KeyCode keycode, bool isRepeat = false) : EventImpl(keycode), m_isRepeat(isRepeat) {}

        bool IsRepeat() const { return m_isRepeat; }

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << m_keyCode << " (repeat = " << m_isRepeat << ")";
            return ss.str();
        }

    private:
        bool m_isRepeat;
    };

    class KeyReleasedEvent : public EventImpl<KeyEvent, KeyReleasedEvent, EventType::KeyReleased, EventCategory::Keyboard | EventCategory::Input>
    {
    public:
        KeyReleasedEvent(const KeyCode keycode) : EventImpl(keycode) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << m_keyCode;
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
            ss << GetName() << ": " << m_keyCode;
            return ss.str();
        }
    };
}