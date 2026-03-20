#pragma once

#include "RenderDevice.h"

#include <cstdint>

namespace Wayfinder
{
    // A sub-allocation from a transient ring buffer.
    struct TransientAllocation
    {
        GPUBufferHandle Buffer{};
        uint32_t Offset = 0;
        uint32_t Size = 0;

        bool IsValid() const { return Buffer.IsValid() && Size > 0; }
    };

    // Per-frame ring-buffer allocator for dynamic vertex/index data.
    // Owns one vertex ring buffer and one index ring buffer.
    // At frame start, the write cursors reset so the entire capacity is reusable.
    // Data is uploaded via the RenderDevice transfer path.
    class WAYFINDER_API TransientBufferAllocator
    {
    public:
        TransientBufferAllocator() = default;
        ~TransientBufferAllocator() = default;

        TransientBufferAllocator(const TransientBufferAllocator&) = delete;
        TransientBufferAllocator& operator=(const TransientBufferAllocator&) = delete;

        bool Initialise(RenderDevice& device, uint32_t vertexCapacity, uint32_t indexCapacity);
        void Shutdown();

        // Call once per frame before any Allocate calls.
        void BeginFrame();

        // Allocate and upload vertex data. Returns a sub-region of the vertex ring buffer.
        TransientAllocation AllocateVertices(const void* data, uint32_t sizeInBytes);

        // Allocate and upload index data. Returns a sub-region of the index ring buffer.
        TransientAllocation AllocateIndices(const void* data, uint32_t sizeInBytes);

    private:
        TransientAllocation AllocateFromRing(GPUBufferHandle ring, uint32_t capacity,
                                             uint32_t& cursor, const void* data, uint32_t sizeInBytes);

        RenderDevice* m_device = nullptr;

        GPUBufferHandle m_vertexRing{};
        GPUBufferHandle m_indexRing{};

        uint32_t m_vertexCapacity = 0;
        uint32_t m_indexCapacity = 0;

        uint32_t m_vertexCursor = 0;
        uint32_t m_indexCursor = 0;
    };

} // namespace Wayfinder
