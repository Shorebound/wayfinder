#pragma once

#include "MipGenerator.h"
#include "core/Log.h"

#include <SDL3/SDL.h>
#include <algorithm>

namespace Wayfinder
{
    /**
     * @brief Mip generator using SDL_BlitGPUTexture (bilinear downsample).
     *
     * Requires the texture to have ColourTarget usage. Acquires and submits
     * its own command buffer so it can be called outside the frame lifecycle.
     */
    class BlitMipGenerator final : public IMipGenerator
    {
    public:
        void Generate(SDL_GPUDevice* device, SDL_GPUTexture* texture, uint32_t mipLevels, uint32_t baseWidth, uint32_t baseHeight) override
        {
            SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(device);
            if (!cmdBuf)
            {
                Log::Error(LogRenderer, "BlitMipGenerator: Failed to acquire command buffer — {}", SDL_GetError());
                return;
            }

            uint32_t srcWidth = baseWidth;
            uint32_t srcHeight = baseHeight;

            for (uint32_t i = 1; i < mipLevels; ++i)
            {
                const uint32_t dstWidth = std::max(1u, srcWidth / 2);
                const uint32_t dstHeight = std::max(1u, srcHeight / 2);

                SDL_GPUBlitInfo blitInfo{};

                blitInfo.source.texture = texture;
                blitInfo.source.mip_level = i - 1;
                blitInfo.source.layer_or_depth_plane = 0;
                blitInfo.source.x = 0;
                blitInfo.source.y = 0;
                blitInfo.source.w = srcWidth;
                blitInfo.source.h = srcHeight;

                blitInfo.destination.texture = texture;
                blitInfo.destination.mip_level = i;
                blitInfo.destination.layer_or_depth_plane = 0;
                blitInfo.destination.x = 0;
                blitInfo.destination.y = 0;
                blitInfo.destination.w = dstWidth;
                blitInfo.destination.h = dstHeight;

                blitInfo.load_op = SDL_GPU_LOADOP_DONT_CARE;
                blitInfo.filter = SDL_GPU_FILTER_LINEAR;
                blitInfo.cycle = false;
                blitInfo.flip_mode = SDL_FLIP_NONE;

                SDL_BlitGPUTexture(cmdBuf, &blitInfo);

                srcWidth = dstWidth;
                srcHeight = dstHeight;
            }

            SDL_SubmitGPUCommandBuffer(cmdBuf);
        }
    };

} // namespace Wayfinder
