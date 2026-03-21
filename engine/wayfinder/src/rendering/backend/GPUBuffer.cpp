#include "GPUBuffer.h"
#include "core/Log.h"

namespace Wayfinder
{
    GPUBuffer::GPUBuffer(GPUBuffer&& other) noexcept
        : m_device(other.m_device), m_handle(other.m_handle), m_size(other.m_size)
    {
        other.m_device = nullptr;
        other.m_handle = {};
        other.m_size = 0;
    }

    GPUBuffer& GPUBuffer::operator=(GPUBuffer&& other) noexcept
    {
        if (this != &other)
        {
            Destroy();
            m_device = other.m_device;
            m_handle = other.m_handle;
            m_size = other.m_size;
            other.m_device = nullptr;
            other.m_handle = {};
            other.m_size = 0;
        }
        return *this;
    }

    bool GPUBuffer::Create(RenderDevice& device, BufferUsage usage, uint32_t sizeInBytes)
    {
        m_device = &device;

        BufferCreateDesc desc{};
        desc.usage = usage;
        desc.sizeInBytes = sizeInBytes;

        m_handle = device.CreateBuffer(desc);
        if (!m_handle)
        {
            WAYFINDER_ERROR(LogRenderer, "GPUBuffer: Failed to create buffer ({} bytes)", sizeInBytes);
            return false;
        }

        m_size = sizeInBytes;
        return true;
    }

    void GPUBuffer::Upload(const void* data, uint32_t sizeInBytes)
    {
        if (!m_device || !m_handle)
        {
            return;
        }

        m_device->UploadToBuffer(m_handle, data, sizeInBytes);
    }

    void GPUBuffer::Destroy()
    {
        if (m_device && m_handle.IsValid())
        {
            m_device->DestroyBuffer(m_handle);
            m_handle = {};
            m_size = 0;
        }
    }

} // namespace Wayfinder
