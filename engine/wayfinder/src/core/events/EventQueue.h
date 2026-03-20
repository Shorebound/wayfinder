#pragma once

#include "core/events/Event.h"

#include <functional>
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
        /** @brief Push a polymorphic event into the queue. */
        void Push(std::unique_ptr<Event> event);

        /**
         * @brief Drain all queued events, invoking the handler for each in FIFO order.
         *
         * The queue is cleared after all events have been dispatched.
         */
        void Drain(const std::function<void(Event&)>& handler);

        /** @brief Discard all queued events without dispatching. */
        void Clear();

        /** @brief Number of events currently queued. */
        [[nodiscard]] size_t Size() const;

        /** @brief True when the queue contains no events. */
        [[nodiscard]] bool IsEmpty() const;

    private:
        std::vector<std::unique_ptr<Event>> m_events;
    };

} // namespace Wayfinder
