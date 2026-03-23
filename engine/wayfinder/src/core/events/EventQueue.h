#pragma once

#include "TypedEventBuffer.h"

#include <cassert>
#include <memory>
#include <span>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Wayfinder
{

    /**
     * @brief Per-frame buffer for deferred event dispatch using typed storage.
     *
     * Events are stored as plain values in per-type contiguous buffers,
     * eliminating per-event heap allocation and the need for virtual Clone().
     * A global insertion order is maintained so that Drain delivers events
     * in FIFO order across all types.
     *
     * Double-buffered internally: events pushed during a Drain cycle are
     * safely deferred to the next cycle.
     */
    class EventQueue
    {
    public:
        EventQueue();

        /**
         * @brief Push a typed event by value into its per-type buffer.
         *
         * The event is copy/move-constructed into contiguous storage.
         * No heap allocation per event; no Clone() required.
         */
        template<typename TEvent>
        void Push(TEvent event)
        {
            auto& buffer = GetOrCreateBuffer<TEvent>();
            buffer.Push(std::move(event));
            m_order.push_back({.m_buffer = &buffer, .m_index = buffer.Size() - 1});
            ++m_totalSize;
        }

        /**
         * @brief Drain all queued events, invoking the handler for each in FIFO order.
         *
         * Events queued by the handler during the drain are appended to the
         * next batch and are not dispatched re-entrantly in the current cycle.
         *
         * Re-entrant calls to Drain are not permitted and will assert.
         * If Clear is called during a drain (e.g. from a handler), the clear
         * is deferred and applied automatically once the drain completes.
         */
        template<typename THandler>
        void Drain(THandler&& handler)
        {
            assert(!m_isDraining && "EventQueue::Drain cannot be called re-entrantly");
            m_isDraining = true;

            // Swap out all active buffers into drain storage.
            for (auto& [_, buf] : m_buffers)
            {
                buf->PrepareForDrain();
            }

            // Swap out the insertion-order list.
            std::vector<OrderEntry> batch;
            batch.swap(m_order);
            m_totalSize = 0;

            // Dispatch each event in its original insertion order.
            for (auto& entry : batch)
            {
                handler(entry.m_buffer->At(entry.m_index));
            }

            // Clean up drain storage and preserve capacity.
            for (auto& [_, buf] : m_buffers)
            {
                buf->FinishDrain();
            }

            if (m_order.capacity() < batch.capacity()) { m_order.reserve(batch.capacity()); }

            m_isDraining = false;

            // If Clear was deferred during the drain, apply it now.
            if (m_pendingClear)
            {
                m_pendingClear = false;
                Clear();
            }
        }

        /**
         * @brief Read all pending events of a specific type.
         *
         * Returns a span over events currently in the active buffer.
         * During a Drain cycle, new events pushed by handlers (including
         * via the deferred-push path) accumulate in the active buffer.
         * After Drain completes, this span reflects those newly enqueued
         * events — it is only empty if no TEvent instances were pushed
         * during the drain.
         */
        template<typename TEvent>
        [[nodiscard]] std::span<const TEvent> Read() const
        {
            auto it = m_buffers.find(std::type_index(typeid(TEvent)));
            if (it == m_buffers.end()) { return {}; }
            auto* typed = static_cast<const TypedEventBuffer<TEvent>*>(it->second.get());
            return typed->Events();
        }

        /** @brief Discard all queued events without dispatching. */
        void Clear();

        /** @brief Number of events currently queued (across all types). */
        [[nodiscard]] size_t Size() const;

        /** @brief True when the queue contains no events. */
        [[nodiscard]] bool IsEmpty() const;

    private:
        static constexpr size_t INITIAL_ORDER_CAPACITY = 64;

        struct OrderEntry
        {
            IEventBuffer* m_buffer;
            size_t m_index;
        };

        template<typename TEvent>
        TypedEventBuffer<TEvent>& GetOrCreateBuffer()
        {
            auto key = std::type_index(typeid(TEvent));
            auto it = m_buffers.find(key);
            if (it != m_buffers.end()) { return static_cast<TypedEventBuffer<TEvent>&>(*it->second); }
            auto [inserted, success] = m_buffers.emplace(key, std::make_unique<TypedEventBuffer<TEvent>>());
            return static_cast<TypedEventBuffer<TEvent>&>(*inserted->second);
        }

        std::unordered_map<std::type_index, std::unique_ptr<IEventBuffer>> m_buffers;
        std::vector<OrderEntry> m_order;
        size_t m_totalSize = 0;
        bool m_isDraining = false;
        bool m_pendingClear = false;
    };

} // namespace Wayfinder
