#pragma once

#include "TypedEventBuffer.h"

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
        template <typename TEvent>
        void Push(TEvent event)
        {
            auto& buffer = GetOrCreateBuffer<TEvent>();
            buffer.Push(std::move(event));
            m_order.push_back({&buffer, buffer.Size() - 1});
            ++m_totalSize;
        }

        /**
         * @brief Drain all queued events, invoking the handler for each in FIFO order.
         *
         * Events queued by the handler during the drain are appended to the
         * next batch and are not dispatched re-entrantly in the current cycle.
         */
        template <typename THandler>
        void Drain(THandler&& handler)
        {
            /// Swap out all active buffers into drain storage.
            for (auto& [_, buf] : m_buffers)
            {
                buf->PrepareForDrain();
            }

            /// Swap out the insertion-order list.
            std::vector<OrderEntry> batch;
            batch.swap(m_order);
            m_totalSize = 0;

            /// Dispatch each event in its original insertion order.
            for (auto& entry : batch)
            {
                handler(entry.Buffer->At(entry.Index));
            }

            /// Clean up drain storage and preserve capacity.
            for (auto& [_, buf] : m_buffers)
            {
                buf->FinishDrain();
            }

            if (m_order.capacity() < batch.capacity())
            {
                m_order.reserve(batch.capacity());
            }
        }

        /**
         * @brief Read all pending events of a specific type.
         *
         * Returns a span of events currently in the buffer (pre-drain).
         * After Drain, the buffer is empty and the span will be empty.
         */
        template <typename TEvent>
        [[nodiscard]] std::span<const TEvent> Read() const
        {
            auto it = m_buffers.find(std::type_index(typeid(TEvent)));
            if (it == m_buffers.end())
            {
                return {};
            }
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
            IEventBuffer* Buffer;
            size_t Index;
        };

        template <typename TEvent>
        TypedEventBuffer<TEvent>& GetOrCreateBuffer()
        {
            auto key = std::type_index(typeid(TEvent));
            auto it = m_buffers.find(key);
            if (it != m_buffers.end())
            {
                return static_cast<TypedEventBuffer<TEvent>&>(*it->second);
            }
            auto [inserted, _] = m_buffers.emplace(
                key, std::make_unique<TypedEventBuffer<TEvent>>());
            return static_cast<TypedEventBuffer<TEvent>&>(*inserted->second);
        }

        std::unordered_map<std::type_index, std::unique_ptr<IEventBuffer>> m_buffers;
        std::vector<OrderEntry> m_order;
        size_t m_totalSize = 0;
    };

} // namespace Wayfinder
