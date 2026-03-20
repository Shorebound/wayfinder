#include "EventQueue.h"

namespace Wayfinder
{

    EventQueue::EventQueue()
    {
        m_events.reserve(INITIAL_CAPACITY);
    }

    void EventQueue::Push(std::unique_ptr<Event> event)
    {
        m_events.push_back(std::move(event));
    }

    void EventQueue::Clear()
    {
        m_events.clear();
    }

    size_t EventQueue::Size() const
    {
        return m_events.size();
    }

    bool EventQueue::IsEmpty() const
    {
        return m_events.empty();
    }

} // namespace Wayfinder
