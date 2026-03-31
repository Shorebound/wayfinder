#include "TransientBufferAllocator.h"
#include "core/Log.h"

namespace Wayfinder
{
    bool TransientBufferAllocator::Initialise(RenderDevice& device, uint32_t vertexCapacity, uint32_t indexCapacity)
    {
        m_device = &device;
        m_vertexCapacity = vertexCapacity;
        m_indexCapacity = indexCapacity;

        BufferCreateDesc vertexDesc{};
        vertexDesc.Usage = BufferUsage::Vertex;
        vertexDesc.SizeInBytes = vertexCapacity;

        m_vertexRing = device.CreateBuffer(vertexDesc);
        if (!m_vertexRing)
        {
            Log::Error(LogRenderer, "TransientBufferAllocator: Failed to create vertex ring buffer ({} bytes)", vertexCapacity);
            return false;
        }

        BufferCreateDesc indexDesc{};
        indexDesc.Usage = BufferUsage::Index;
        indexDesc.SizeInBytes = indexCapacity;

        m_indexRing = device.CreateBuffer(indexDesc);
        if (!m_indexRing)
        {
            Log::Error(LogRenderer, "TransientBufferAllocator: Failed to create index ring buffer ({} bytes)", indexCapacity);
            device.DestroyBuffer(m_vertexRing);
            m_vertexRing = {};
            return false;
        }

        Log::Info(LogRenderer, "TransientBufferAllocator initialised (vertex: {} KB, index: {} KB)", vertexCapacity / 1024, indexCapacity / 1024);

        return true;
    }

    void TransientBufferAllocator::Shutdown()
    {
        if (m_device)
        {
            if (m_vertexRing)
            {
                m_device->DestroyBuffer(m_vertexRing);
            }
            if (m_indexRing)
            {
                m_device->DestroyBuffer(m_indexRing);
            }
        }

        m_vertexRing = {};
        m_indexRing = {};
        m_device = nullptr;
        m_vertexCapacity = 0;
        m_indexCapacity = 0;
        m_vertexCursor = 0;
        m_indexCursor = 0;
    }

    void TransientBufferAllocator::BeginFrame()
    {
        m_vertexCursor = 0;
        m_indexCursor = 0;
    }

    TransientAllocation TransientBufferAllocator::AllocateVertices(const void* data, uint32_t sizeInBytes)
    {
        return AllocateFromRing(m_vertexRing, m_vertexCapacity, m_vertexCursor, data, sizeInBytes);
    }

    TransientAllocation TransientBufferAllocator::AllocateIndices(const void* data, uint32_t sizeInBytes)
    {
        return AllocateFromRing(m_indexRing, m_indexCapacity, m_indexCursor, data, sizeInBytes);
    }

    TransientAllocation TransientBufferAllocator::AllocateFromRing(GPUBufferHandle ring, uint32_t capacity, uint32_t& cursor, const void* data, uint32_t sizeInBytes)
    {
        if (!m_device || !ring || !data || sizeInBytes == 0)
        {
            return {};
        }

        // Align cursor to GPU minimum buffer offset alignment (256 bytes for Vulkan)
        static constexpr uint32_t K_MIN_ALIGNMENT = 256;
        const uint32_t alignedCursor = (cursor + K_MIN_ALIGNMENT - 1) & ~(K_MIN_ALIGNMENT - 1);

        if (alignedCursor + sizeInBytes > capacity)
        {
            Log::Warn(LogRenderer, "TransientBufferAllocator: Ring buffer overflow ({} + {} > {} bytes)", alignedCursor, sizeInBytes, capacity);
            return {};
        }

        const uint32_t offset = alignedCursor;
        m_device->UploadToBuffer(ring, data, {.SizeInBytes = sizeInBytes, .DstOffsetInBytes = offset});
        cursor = alignedCursor + sizeInBytes;

        return {.Buffer = ring, .Offset = offset, .Size = sizeInBytes};
    }

} // namespace Wayfinder
