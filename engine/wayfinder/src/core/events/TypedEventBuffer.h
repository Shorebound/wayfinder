#pragma once

#include "Event.h"

#include <span>
#include <vector>

namespace Wayfinder
{

    /**
     * @brief Type-erased interface for per-type event buffers.
     *
     * Used internally by EventQueue to manage heterogeneous typed buffers
     * through a uniform interface during drain and clear operations.
     */
    class IEventBuffer
    {
    public:
        virtual ~IEventBuffer() = default;

        /** @brief Access the Nth event as its Event base for polymorphic dispatch. */
        virtual Event& At(size_t index) = 0;

        /** @brief Swap active storage into drain storage, leaving active empty for new pushes. */
        virtual void PrepareForDrain() = 0;

        /** @brief Discard drained events and preserve capacity for the next frame. */
        virtual void FinishDrain() = 0;

        /** @brief Discard all events (both active and draining). */
        virtual void Clear() = 0;

        /** @brief Number of events in active storage. */
        [[nodiscard]] virtual size_t Size() const = 0;
    };

    /**
     * @brief Per-type contiguous event buffer.
     *
     * Stores events of a single concrete type as plain values in a
     * std::vector, eliminating per-event heap allocation and the need
     * for virtual Clone(). Double-buffered so that events pushed during
     * a Drain cycle are safely deferred to the next cycle.
     *
     * @tparam TEvent  Concrete event type (must derive from Event).
     */
    template<typename TEvent>
    class TypedEventBuffer final : public IEventBuffer
    {
        static_assert(std::is_base_of_v<Event, TEvent>, "TEvent must derive from Event");

    public:
        /** @brief Push an event by copy into active storage. */
        void Push(const TEvent& event)
        {
            m_active.push_back(event);
        }

        /** @brief Push an event by move into active storage. */
        void Push(TEvent&& event)
        {
            m_active.push_back(std::move(event));
        }

        /** @brief Construct an event in-place in active storage. */
        template<typename... TArgs>
        TEvent& Emplace(TArgs&&... args)
        {
            return m_active.emplace_back(std::forward<TArgs>(args)...);
        }

        /** @brief Read-only span of events in active storage (pre-drain). */
        [[nodiscard]] std::span<const TEvent> Events() const
        {
            return m_active;
        }

        // ── IEventBuffer ────────────────────────────────────

        Event& At(size_t index) override
        {
            return m_draining[index];
        }

        void PrepareForDrain() override
        {
            m_draining.swap(m_active);
        }

        void FinishDrain() override
        {
            if (m_active.capacity() < m_draining.capacity())
            {
                m_active.reserve(m_draining.capacity());
            }
            m_draining.clear();
        }

        void Clear() override
        {
            m_active.clear();
            m_draining.clear();
        }

        [[nodiscard]] size_t Size() const override
        {
            return m_active.size();
        }

    private:
        std::vector<TEvent> m_active;   ///< Events pushed this frame.
        std::vector<TEvent> m_draining; ///< Events being drained (previous batch).
    };

} // namespace Wayfinder
