#include "EventQueue.h"

namespace Wayfinder
{

    EventQueue::EventQueue() { m_order.reserve(INITIAL_ORDER_CAPACITY); }

    void EventQueue::Clear()
    {
        if (m_isDraining)
        {
            m_pendingClear = true;
            return;
        }

        for (auto& [_, buf] : m_buffers)
        {
            buf->Clear();
        }
        m_order.clear();
        m_totalSize = 0;
    }

    size_t EventQueue::Size() const { return m_totalSize; }

    bool EventQueue::IsEmpty() const { return m_totalSize == 0; }

} // namespace Wayfinder
