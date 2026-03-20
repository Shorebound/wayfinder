#pragma once

#include "Handle.h"

#include <cassert>
#include <cstdint>
#include <vector>

namespace Wayfinder
{
    /**
     * @brief A pool that maps generational handles to resources.
     *
     * Manages index→resource mappings, validates generations on access to catch
     * use-after-free at runtime, and maintains a free list for efficient slot reuse.
     *
     * @tparam TTag  The handle tag type (determines the Handle<TTag> domain).
     * @tparam TResource  The concrete resource type stored in each slot.
     */
    template <typename TTag, typename TResource>
    class ResourcePool
    {
    public:
        using HandleType = Handle<TTag>;

        ResourcePool() = default;
        ResourcePool(const ResourcePool&) = delete;
        ResourcePool& operator=(const ResourcePool&) = delete;
        ResourcePool(ResourcePool&&) = default;
        ResourcePool& operator=(ResourcePool&&) = default;

        /**
         * @brief Acquires a new slot and stores the resource.
         * @param resource The resource to store (moved into the pool).
         * @return A valid handle referencing the new slot.
         */
        HandleType Acquire(TResource resource)
        {
            uint32_t index = 0;

            if (!m_freeList.empty())
            {
                index = m_freeList.back();
                m_freeList.pop_back();
            }
            else
            {
                if (m_entries.size() >= MAX_INDEX)
                {
                    assert(false && "ResourcePool: exceeded maximum slot count");
                    return HandleType{};
                }
                index = static_cast<uint32_t>(m_entries.size());
                m_entries.emplace_back();
            }

            auto& entry = m_entries[index];
            entry.Resource = std::move(resource);

            /// Initialise generation to 1 for new entries (generation 0 is reserved as invalid).
            /// Generation increments happen in Release(), not here.
            entry.Generation = (entry.Generation == 0) ? 1 : entry.Generation;
            entry.Alive = true;
            ++m_activeCount;

            HandleType handle{};
            handle.Index = index;
            handle.Generation = entry.Generation;
            return handle;
        }

        /**
         * @brief Releases the resource at the given handle, bumping the generation to
         *        invalidate any outstanding handles to the same slot.
         */
        void Release(HandleType handle)
        {
            if (!IsValid(handle)) return;

            auto& entry = m_entries[handle.Index];
            entry.Resource = TResource{};
            entry.Alive = false;
            --m_activeCount;

            /// Bump generation so stale handles to this slot fail validation.
            entry.Generation = (entry.Generation + 1) & MAX_GENERATION;
            if (entry.Generation == 0) entry.Generation = 1;

            m_freeList.push_back(handle.Index);
        }

        /**
         * @brief Checks if a handle is currently valid (alive and generation matches).
         */
        [[nodiscard]] bool IsValid(HandleType handle) const
        {
            if (!handle.IsValid()) return false;
            if (handle.Index >= m_entries.size()) return false;

            const auto& entry = m_entries[handle.Index];
            return entry.Alive && entry.Generation == handle.Generation;
        }

        /**
         * @brief Returns a pointer to the stored resource, or nullptr if the handle is stale/invalid.
         */
        [[nodiscard]] TResource* Get(HandleType handle)
        {
            if (!IsValid(handle)) return nullptr;
            return &m_entries[handle.Index].Resource;
        }

        /**
         * @brief Returns a const pointer to the stored resource, or nullptr if the handle is stale/invalid.
         */
        [[nodiscard]] const TResource* Get(HandleType handle) const
        {
            if (!IsValid(handle)) return nullptr;
            return &m_entries[handle.Index].Resource;
        }

        /**
         * @brief Returns the number of currently alive slots.
         */
        [[nodiscard]] size_t ActiveCount() const
        {
            return m_activeCount;
        }

        /**
         * @brief Calls fn(TResource&) for each alive entry.
         */
        template <typename TFn>
        void ForEachAlive(TFn&& fn)
        {
            for (auto& entry : m_entries)
            {
                if (entry.Alive) fn(entry.Resource);
            }
        }

        /**
         * @brief Clears all entries, preserving per-slot generation counters so that
         *        handles issued before Clear() cannot pass validation after it.
         *        Caller should Release() or otherwise clean up GPU resources before calling Clear().
         */
        void Clear()
        {
            m_freeList.clear();
            m_activeCount = 0;
            for (uint32_t i = 0; i < m_entries.size(); ++i)
            {
                auto& entry = m_entries[i];
                entry.Resource = TResource{};
                entry.Alive = false;
                entry.Generation = (entry.Generation + 1) & MAX_GENERATION;
                if (entry.Generation == 0) entry.Generation = 1;
                m_freeList.push_back(i);
            }
        }

    private:
        static constexpr uint32_t MAX_INDEX      = (1u << 20);       // ~1M slots
        static constexpr uint32_t MAX_GENERATION  = (1u << 12) - 1;  // 4095

        struct Entry
        {
            TResource Resource{};
            uint32_t Generation = 0;
            bool Alive = false;
        };

        std::vector<Entry> m_entries;
        std::vector<uint32_t> m_freeList;
        size_t m_activeCount = 0;
    };

} // namespace Wayfinder
