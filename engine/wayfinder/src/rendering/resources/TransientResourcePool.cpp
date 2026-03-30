#include "TransientResourcePool.h"
#include "core/Log.h"
#include "rendering/backend/RenderDevice.h"

namespace Wayfinder
{
    void TransientResourcePool::Initialise(RenderDevice& device)
    {
        m_device = &device;
    }

    void TransientResourcePool::Shutdown()
    {
        if (!m_device)
        {
            return;
        }

        for (const GPUTextureHandle tex : m_allTextures)
        {
            m_device->DestroyTexture(tex);
        }
        m_allTextures.clear();
        m_available.clear();
        m_device = nullptr;
    }

    GPUTextureHandle TransientResourcePool::Acquire(const TextureCreateDesc& desc)
    {
        if (!m_device)
        {
            Log::Error(LogRenderer, "TransientResourcePool::Acquire called before Initialise");
            return GPUTextureHandle::Invalid();
        }

        const PoolKey key{.Width = desc.Width, .Height = desc.Height, .Format = desc.Format, .Usage = desc.Usage};
        auto it = m_available.find(key);
        if (it != m_available.end() && !it->second.empty())
        {
            GPUTextureHandle tex = it->second.back();
            it->second.pop_back();
            return tex;
        }

        // Create a new texture
        GPUTextureHandle tex = m_device->CreateTexture(desc);
        if (tex)
        {
            m_allTextures.push_back(tex);
        }
        return tex;
    }

    void TransientResourcePool::Release(GPUTextureHandle texture, const TextureCreateDesc& desc)
    {
        if (!texture.IsValid())
        {
            return;
        }
        if (!m_device)
        {
            return;
        }

        const PoolKey key{.Width = desc.Width, .Height = desc.Height, .Format = desc.Format, .Usage = desc.Usage};
        m_available[key].push_back(texture);
    }

} // namespace Wayfinder
