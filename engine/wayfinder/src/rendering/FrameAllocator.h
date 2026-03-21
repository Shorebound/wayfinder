#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace Wayfinder
{
    /**
     * @brief Per-frame bump/arena allocator.
     *
     * Provides O(1) allocation via pointer bumping with configurable page size.
     * All allocations remain valid until Reset() is called. Non-trivially-destructible
     * objects created via Create<T>() have their destructors registered and called
     * in LIFO order during Reset().
     *
     * Typical usage: one instance per frame, Reset() once at frame start.
     */
    class FrameAllocator
    {
    public:
        static constexpr size_t DEFAULT_PAGE_SIZE = 64 * 1024; // 64 KB

        explicit FrameAllocator(size_t pageSize = DEFAULT_PAGE_SIZE);
        ~FrameAllocator();

        FrameAllocator(const FrameAllocator&) = delete;
        FrameAllocator& operator=(const FrameAllocator&) = delete;
        FrameAllocator(FrameAllocator&&) = delete;
        FrameAllocator& operator=(FrameAllocator&&) = delete;

        /**
         * @brief Allocate raw bytes with the given alignment.
         * @return Aligned pointer into arena memory. Never null (asserts on failure).
         */
        void* Allocate(size_t bytes, size_t alignment = alignof(std::max_align_t));

        /**
         * @brief Placement-new a T into the arena.
         *
         * If T is non-trivially destructible, its destructor is registered and
         * will be called during Reset() in LIFO order.
         *
         * @return Pointer to the constructed object. The arena owns the memory.
         */
        template <typename T, typename... TArgs>
        T* Create(TArgs&&... args);

        /**
         * @brief Call all registered destructors (LIFO), then reset the bump pointer.
         *
         * Pages are retained for reuse — no deallocation occurs.
         */
        void Reset();

        /** @brief Total bytes used across all pages since last Reset(). */
        size_t GetUsedBytes() const;

        /** @brief Total capacity across all allocated pages. */
        size_t GetCapacity() const;

    private:
        using DestroyFn = void (*)(void*);

        struct DestructorEntry
        {
            DestroyFn Destroy;
            void* Object;
        };

        struct Page
        {
            std::unique_ptr<std::byte[]> Memory;
            size_t Capacity;
        };

        void AddPage(size_t minCapacity);
        static size_t AlignUp(size_t value, size_t alignment);

        std::vector<Page> m_pages;
        size_t m_pageSize;
        size_t m_currentPage = 0;
        size_t m_currentOffset = 0;
        std::vector<DestructorEntry> m_destructors;
    };

    // ── Template Implementation ──────────────────────────────

    template <typename T, typename... TArgs>
    T* FrameAllocator::Create(TArgs&&... args)
    {
        void* storage = Allocate(sizeof(T), alignof(T));
        T* object = ::new (storage) T(std::forward<TArgs>(args)...);

        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            m_destructors.push_back({
                [](void* ptr) { static_cast<T*>(ptr)->~T(); },
                object
            });
        }

        return object;
    }

} // namespace Wayfinder
