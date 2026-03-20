#pragma once

#include <functional>
#include <memory>
#include <ostream>
#include <string>

namespace Wayfinder
{

    // Input events use deferred dispatch via EventQueue — they are buffered
    // during SDL polling and drained at a well-defined point in the frame loop.
    // Latency-sensitive events (window close, resize) still dispatch immediately.

    enum class EventType
    {
        None = 0,
        WindowClose,
        WindowResize,
        WindowFocus,
        WindowLostFocus,
        WindowMoved,
        AppTick,
        AppUpdate,
        AppRender,
        KeyPressed,
        KeyReleased,
        KeyTyped,
        MouseButtonPressed,
        MouseButtonReleased,
        MouseMoved,
        MouseScrolled
    };

    constexpr const char* EventTypeToString(EventType type)
    {
        switch (type)
        {
        // Application Events
        case EventType::AppTick:
            return "AppTick";
        case EventType::AppUpdate:
            return "AppUpdate";
        case EventType::AppRender:
            return "AppRender";
        // Keyboard Events
        case EventType::KeyPressed:
            return "KeyPressed";
        case EventType::KeyReleased:
            return "KeyReleased";
        case EventType::KeyTyped:
            return "KeyTyped";
        // Mouse Events
        case EventType::MouseButtonPressed:
            return "MouseButtonPressed";
        case EventType::MouseButtonReleased:
            return "MouseButtonReleased";
        case EventType::MouseMoved:
            return "MouseMoved";
        case EventType::MouseScrolled:
            return "MouseScrolled";
        default:
            // Using throw might be better during development
            // throw std::runtime_error("Unknown EventType in EventTypeToString");
            return "UnknownEvent";
        }
    }

    enum class EventCategory
    {
        None = 0,
        Application = 1 << 0,
        Input = 1 << 1,
        Keyboard = 1 << 2,
        Mouse = 1 << 3,
        MouseButton = 1 << 4
    };

    // Add operator overloads for enum class bitwise operations
    inline constexpr EventCategory operator|(EventCategory a, EventCategory b)
    {
        return static_cast<EventCategory>(static_cast<std::underlying_type_t<EventCategory>>(a) | static_cast<std::underlying_type_t<EventCategory>>(b));
    }

    inline constexpr EventCategory operator&(EventCategory a, EventCategory b)
    {
        return static_cast<EventCategory>(static_cast<std::underlying_type_t<EventCategory>>(a) & static_cast<std::underlying_type_t<EventCategory>>(b));
    }

    inline constexpr EventCategory& operator|=(EventCategory& a, EventCategory b)
    {
        a = a | b;
        return a;
    }

    inline constexpr EventCategory& operator&=(EventCategory& a, EventCategory b)
    {
        a = a & b;
        return a;
    }

    class Event
    {
    public:
        virtual ~Event() = default;

        bool Handled = false;

        virtual EventType GetEventType() const = 0;
        virtual const char* GetName() const = 0;
        virtual EventCategory GetCategoryFlags() const = 0;
        virtual std::string ToString() const { return GetName(); }

        /** @brief Create a heap-allocated copy of this event for deferred dispatch. */
        virtual std::unique_ptr<Event> Clone() const = 0;

        bool IsInCategory(const EventCategory category) const
        {
            return (GetCategoryFlags() & category) != EventCategory::None;
        }
    };

    class EventDispatcher
    {
    public:
        EventDispatcher(Event& event) : m_event(event) { }

        // F will be deduced by the compiler
        template <typename T, typename F>
        bool Dispatch(const F& func)
        {
            if (m_event.GetEventType() == T::GetStaticType())
            {
                m_event.Handled |= func(static_cast<T&>(m_event));
                return true;
            }
            return false;
        }

    private:
        Event& m_event;
    };

    inline std::ostream& operator<<(std::ostream& os, const Event& e)
    {
        return os << e.ToString();
    }

    template <typename Base, EventType TTypeValue, EventCategory TCategoryValue>
    class EventImpl : public Base
    {
        static_assert(std::is_base_of_v<Event, Base>, "Base must inherit from Event");

    public:
        using Base::Base; // Inherit constructors

        static constexpr EventType GetStaticType() noexcept { return TTypeValue; }
        static constexpr EventCategory GetStaticCategory() noexcept { return TCategoryValue; }

        virtual EventType GetEventType() const override { return GetStaticType(); }
        virtual const char* GetName() const override { return EventTypeToString(GetStaticType()); }
        virtual EventCategory GetCategoryFlags() const override { return GetStaticCategory(); }
    };

}