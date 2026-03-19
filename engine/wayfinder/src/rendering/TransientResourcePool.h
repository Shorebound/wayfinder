#pragma once

#include "RenderTypes.h"

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
        ~TransientResourcePool() = default;

        void Initialize(RenderDevice& device);
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
                size_t h = std::hash<uint32_t>{}(k.Width);
                h ^= std::hash<uint32_t>{}(k.Height) << 1;
                h ^= std::hash<uint8_t>{}(static_cast<uint8_t>(k.Format)) << 2;
                h ^= std::hash<uint32_t>{}(static_cast<uint32_t>(k.Usage)) << 3;
                return h;
            }
        };

        RenderDevice* m_device = nullptr;
        std::unordered_map<PoolKey, std::vector<GPUTextureHandle>, PoolKeyHash> m_available;
        std::vector<GPUTextureHandle> m_allTextures;
    };

} // namespace Wayfinder
