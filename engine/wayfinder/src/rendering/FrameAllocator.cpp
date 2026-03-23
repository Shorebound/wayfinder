#include "FrameAllocator.h"

#include "core/Assert.h"

#include <algorithm>

namespace Wayfinder
{
    FrameAllocator::FrameAllocator(size_t pageSize) : m_pageSize(pageSize) { AddPage(m_pageSize); }

    FrameAllocator::~FrameAllocator() { Reset(); }

    void* FrameAllocator::Allocate(size_t bytes, size_t alignment)
    {
        WAYFINDER_ASSERT(bytes > 0, "FrameAllocator::Allocate: zero-byte allocation");

        // Try to fit in the current page (align actual address, not just offset)
        auto& page = m_pages[m_currentPage];
        const uintptr_t base = reinterpret_cast<uintptr_t>(page.Memory.get());
        const uintptr_t current = base + m_currentOffset;
        const uintptr_t aligned = AlignUp(current, alignment);
        const size_t padding = aligned - current;

        if (m_currentOffset + padding + bytes <= page.Capacity)
        {
            m_currentOffset += padding + bytes;
            return reinterpret_cast<void*>(aligned);
        }

        // Current page exhausted — try next existing page or allocate a new one
        const size_t needed = bytes + alignment - 1; // Worst-case with alignment padding
        size_t nextPage = m_currentPage + 1;
        if (nextPage < m_pages.size() && needed <= m_pages[nextPage].Capacity)
        {
            m_currentPage = nextPage;
            const uintptr_t nextBase = reinterpret_cast<uintptr_t>(m_pages[nextPage].Memory.get());
            const uintptr_t nextAligned = AlignUp(nextBase, alignment);
            m_currentOffset = (nextAligned - nextBase) + bytes;
            return reinterpret_cast<void*>(nextAligned);
        }

        // Need a new page at least as large as the request (+alignment headroom)
        const size_t newPageSize = std::max(m_pageSize, needed);
        AddPage(newPageSize);
        m_currentPage = m_pages.size() - 1;

        const uintptr_t newBase = reinterpret_cast<uintptr_t>(m_pages.back().Memory.get());
        const uintptr_t newAligned = AlignUp(newBase, alignment);
        m_currentOffset = (newAligned - newBase) + bytes;
        return reinterpret_cast<void*>(newAligned);
    }

    void FrameAllocator::Reset()
    {
        // Destroy in LIFO order (head is most recently registered)
        DestructorEntry* current = m_destructorHead;
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
        for (size_t i = 0; i < m_currentPage; ++i)
        {
            total += m_pages[i].Capacity;
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
        page.Memory = std::make_unique<std::byte[]>(minCapacity);
        m_pages.push_back(std::move(page));
    }

    size_t FrameAllocator::AlignUp(size_t value, size_t alignment)
    {
        WAYFINDER_ASSERT(
            alignment != 0 && (alignment & (alignment - 1)) == 0, "AlignUp: alignment must be a non-zero power of two (got {})", alignment);
        return (value + alignment - 1) & ~(alignment - 1);
    }

} // namespace Wayfinder
