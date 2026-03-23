#pragma once

#include "rendering/RenderTypes.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Wayfinder
{
    class RenderDevice;

    // Pools GPU textures for render graph transient resources.
    // Textures are recycled across frames by matching format and dimensions.
    // The pool owns all texture handles and destroys them on Shutdown.
    class TransientResourcePool
    {
    public:
        TransientResourcePool() = default;
        ~TransientResourcePool()
        {
            Shutdown();
        }

        TransientResourcePool(const TransientResourcePool&) = delete;
        TransientResourcePool& operator=(const TransientResourcePool&) = delete;

        TransientResourcePool(TransientResourcePool&& other) noexcept : m_device(other.m_device), m_available(std::move(other.m_available)), m_allTextures(std::move(other.m_allTextures))
        {
            other.m_device = nullptr;
        }

        TransientResourcePool& operator=(TransientResourcePool&& other) noexcept
        {
            if (this != &other)
            {
                Shutdown();
                m_device = other.m_device;
                m_available = std::move(other.m_available);
                m_allTextures = std::move(other.m_allTextures);
                other.m_device = nullptr;
            }
            return *this;
        }

        void Initialise(RenderDevice& device);
        void Shutdown();

        // Returns a texture matching the description, creating one if no match is available.
        GPUTextureHandle Acquire(const TextureCreateDesc& desc);

        // Returns a texture to the pool for future reuse.
        void Release(GPUTextureHandle texture, const TextureCreateDesc& desc);

    private:
        struct PoolKey
        {
            uint32_t Width;
            uint32_t Height;
            TextureFormat Format;
            TextureUsage Usage;

            bool operator==(const PoolKey&) const = default;
        };

        struct PoolKeyHash
        {
            size_t operator()(const PoolKey& k) const
            {
                size_t h = 17;
                h = h * 31 + std::hash<uint32_t>{}(k.Width);
                h = h * 31 + std::hash<uint32_t>{}(k.Height);
                h = h * 31 + static_cast<size_t>(k.Format);
                h = h * 31 + static_cast<size_t>(k.Usage);
                return h;
            }
        };

        RenderDevice* m_device = nullptr;
        std::unordered_map<PoolKey, std::vector<GPUTextureHandle>, PoolKeyHash> m_available;
        std::vector<GPUTextureHandle> m_allTextures;
    };

} // namespace Wayfinder
