#pragma once

#include "Event.h"

#include <memory>
#include <vector>

namespace Wayfinder
{

    /**
     * @brief Per-frame buffer for deferred event dispatch.
     *
     * Events are pushed during SDL polling and drained at a well-defined
     * point in the frame loop. This decouples event producers from handler
     * execution order and enables future deterministic replay.
     *
     * Initial storage uses std::vector<std::unique_ptr<Event>>. Once a
     * frame-linear allocator is available, this can be replaced with
     * arena-backed storage to eliminate per-event heap allocation.
     */
    class EventQueue
    {
    public:
        EventQueue();

        /** @brief Push a polymorphic event into the queue. */
        void Push(std::unique_ptr<Event> event);

        /**
         * @brief Drain all queued events, invoking the handler for each in FIFO order.
         *
         * Events queued by the handler are appended to the next batch and are not
         * dispatched re-entrantly in the current drain cycle.
         */
        template <typename THandler>
        void Drain(THandler&& handler)
        {
            std::vector<std::unique_ptr<Event>> batch;
            batch.swap(m_events);

            for (auto& event : batch)
            {
                handler(*event);
            }

            if (m_events.capacity() < batch.capacity())
            {
                m_events.reserve(batch.capacity());
            }
        }

        /** @brief Discard all queued events without dispatching. */
        void Clear();

        /** @brief Number of events currently queued. */
        [[nodiscard]] size_t Size() const;

        /** @brief True when the queue contains no events. */
        [[nodiscard]] bool IsEmpty() const;

    private:
        static constexpr size_t INITIAL_CAPACITY = 64;
        std::vector<std::unique_ptr<Event>> m_events;
    };

} // namespace Wayfinder
