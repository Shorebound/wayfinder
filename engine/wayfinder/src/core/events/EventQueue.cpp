#include "EventQueue.h"

namespace Wayfinder
{

    void EventQueue::Push(std::unique_ptr<Event> event)
    {
        m_events.push_back(std::move(event));
    }

    void EventQueue::Drain(const std::function<void(Event&)>& handler)
    {
        for (auto& event : m_events)
        {
            handler(*event);
        }
        m_events.clear();
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
