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
                assert(m_entries.size() < MAX_INDEX && "ResourcePool: exceeded maximum slot count");
                index = static_cast<uint32_t>(m_entries.size());
                m_entries.emplace_back();
            }

            auto& entry = m_entries[index];
            entry.Resource = std::move(resource);

            /// Bump generation on every acquire. Start from 1 so generation 0 is always invalid.
            entry.Generation = (entry.Generation == 0) ? 1 : entry.Generation;
            entry.Alive = true;

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
            size_t count = 0;
            for (const auto& entry : m_entries)
            {
                if (entry.Alive) ++count;
            }
            return count;
        }

        /**
         * @brief Clears all entries and the free list. Does not call destructors on stored resources
         *        beyond what the vector clear provides — caller should Release() or otherwise clean
         *        up resources before calling Clear().
         */
        void Clear()
        {
            m_entries.clear();
            m_freeList.clear();
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
    };

} // namespace Wayfinder
