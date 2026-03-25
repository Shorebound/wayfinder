#pragma once

#include <cstdint>

struct SDL_GPUDevice;
struct SDL_GPUTexture;

namespace Wayfinder
{
    /**
     * @brief Strategy interface for GPU mipmap generation.
     *
     * Implementations choose how to downsample mip level 0 into all
     * subsequent levels (blit, compute, etc.).
     */
    class IMipGenerator
    {
    public:
        virtual ~IMipGenerator() = default;

        /**
         * @brief Generate mip levels 1..mipLevels-1 from level 0.
         * @param device      SDL GPU device (non-null).
         * @param texture     SDL texture to generate mips for.
         * @param mipLevels   Total mip level count (must be > 1).
         * @param baseWidth   Width of mip level 0.
         * @param baseHeight  Height of mip level 0.
         */
        virtual void Generate(SDL_GPUDevice* device, SDL_GPUTexture* texture, uint32_t mipLevels, uint32_t baseWidth, uint32_t baseHeight) = 0;
    };

} // namespace Wayfinder
