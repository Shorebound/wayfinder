#include "FrameAllocator.h"

#include "core/Assert.h"

#include <algorithm>
#include <bit>
#include <memory>

namespace Wayfinder
{
    FrameAllocator::FrameAllocator(size_t pageSize) : m_pageSize(pageSize)
    {
        AddPage(m_pageSize);
    }

    FrameAllocator::~FrameAllocator()
    {
        Reset();
    }

    void* FrameAllocator::Allocate(size_t bytes, size_t alignment)
    {
        WAYFINDER_ASSERT(bytes > 0, "FrameAllocator::Allocate: zero-byte allocation");
        const bool hasValidAlignment = alignment != 0 && std::has_single_bit(alignment);
        WAYFINDER_ASSERT(hasValidAlignment, "FrameAllocator::Allocate: alignment must be a non-zero power of two (got {})", alignment);

        auto tryAllocateFromPage = [bytes, alignment](Page& page, size_t& currentOffset) -> void*
        {
            void* currentPtr = page.Memory.data() + currentOffset;
            size_t remainingSpace = page.Capacity - currentOffset;
            void* alignedPtr = std::align(alignment, bytes, currentPtr, remainingSpace);
            if (!alignedPtr)
            {
                return nullptr;
            }

            const size_t alignedOffset = page.Capacity - remainingSpace;
            currentOffset = alignedOffset + bytes;
            return alignedPtr;
        };

        if (void* allocation = tryAllocateFromPage(m_pages.at(m_currentPage), m_currentOffset))
        {
            return allocation;
        }

        // Current page exhausted — try next existing page or allocate a new one
        const size_t needed = bytes + alignment - 1; // Worst-case with alignment padding
        const size_t nextPage = m_currentPage + 1;
        if (nextPage < m_pages.size() && needed <= m_pages.at(nextPage).Capacity)
        {
            m_currentPage = nextPage;
            m_currentOffset = 0;
            if (void* allocation = tryAllocateFromPage(m_pages.at(m_currentPage), m_currentOffset))
            {
                return allocation;
            }
        }

        // Need a new page at least as large as the request (+alignment headroom)
        const size_t newPageSize = std::max(m_pageSize, needed);
        AddPage(newPageSize);
        m_currentPage = m_pages.size() - 1;
        m_currentOffset = 0;
        void* allocation = tryAllocateFromPage(m_pages.back(), m_currentOffset);
        WAYFINDER_ASSERT(allocation != nullptr, "FrameAllocator::Allocate: failed to allocate {} bytes from a fresh page of size {}", bytes, newPageSize);
        return allocation;
    }

    void FrameAllocator::Reset()
    {
        // Destroy in LIFO order (head is most recently registered)
        const DestructorEntry* current = m_destructorHead;
        while (current)
        {
            current->Destroy(current->Object);
            current = current->Next;
        }
        m_destructorHead = nullptr;

        m_currentPage = 0;
        m_currentOffset = 0;
    }

    size_t FrameAllocator::GetUsedBytes() const
    {
        size_t total = 0;
        for (size_t pageIndex = 0; pageIndex < m_currentPage; ++pageIndex)
        {
            total += m_pages.at(pageIndex).Capacity;
        }
        total += m_currentOffset;
        return total;
    }

    size_t FrameAllocator::GetCapacity() const
    {
        size_t total = 0;
        for (const auto& page : m_pages)
        {
            total += page.Capacity;
        }
        return total;
    }

    void FrameAllocator::AddPage(size_t minCapacity)
    {
        Page page;
        page.Capacity = minCapacity;
        page.Memory.resize(minCapacity);
        m_pages.push_back(std::move(page));
    }

} // namespace Wayfinder
