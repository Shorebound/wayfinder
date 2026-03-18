#pragma once

#include "RenderDevice.h"

namespace Wayfinder
{
    // RAII wrapper for a GPU buffer (vertex or index).
    // Manages creation, upload, and destruction through the abstract RenderDevice.
    class WAYFINDER_API GPUBuffer
    {
    public:
        GPUBuffer() = default;
        ~GPUBuffer() = default;

        GPUBuffer(const GPUBuffer&) = delete;
        GPUBuffer& operator=(const GPUBuffer&) = delete;

        GPUBuffer(GPUBuffer&& other) noexcept;
        GPUBuffer& operator=(GPUBuffer&& other) noexcept;

        bool Create(RenderDevice& device, BufferUsage usage, uint32_t sizeInBytes);
        void Upload(const void* data, uint32_t sizeInBytes);
        void Destroy();

        GPUBufferHandle GetHandle() const { return m_handle; }
        bool IsValid() const { return m_handle != nullptr; }
        uint32_t GetSize() const { return m_size; }

    private:
        RenderDevice* m_device = nullptr;
        GPUBufferHandle m_handle = nullptr;
        uint32_t m_size = 0;
    };

} // namespace Wayfinder
