#include "TransientResourcePool.h"
#include "RenderDevice.h"
#include "../core/Log.h"

namespace Wayfinder
{
    void TransientResourcePool::Initialize(RenderDevice& device) { m_device = &device; }

    void TransientResourcePool::Shutdown()
    {
        if (!m_device) return;

        for (GPUTextureHandle tex : m_allTextures)
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
            WAYFINDER_ERROR(LogRenderer, "TransientResourcePool::Acquire called before Initialize");
            return nullptr;
        }

        PoolKey key{desc.width, desc.height, desc.format, desc.usage};
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
        if (!texture) return;

        PoolKey key{desc.width, desc.height, desc.format, desc.usage};
        m_available[key].push_back(texture);
    }

} // namespace Wayfinder
