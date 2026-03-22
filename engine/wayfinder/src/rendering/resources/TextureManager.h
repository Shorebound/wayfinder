#pragma once

#include <string>
#include <unordered_map>

#include "assets/AssetService.h"
#include "assets/TextureAsset.h"
#include "rendering/backend/GPUHandles.h"
#include "rendering/RenderTypes.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    class RenderDevice;

    /**
     * @brief Manages GPU texture lifetimes, fallback textures, and sampler deduplication.
     *
     * Owns all GPU texture and sampler handles created for asset-loaded textures.
     * Created once at initialisation as part of RenderContext.
     */
    class WAYFINDER_API TextureManager
    {
    public:
        TextureManager() = default;
        ~TextureManager() = default;

        TextureManager(const TextureManager&) = delete;
        TextureManager& operator=(const TextureManager&) = delete;
        TextureManager(TextureManager&&) = delete;
        TextureManager& operator=(TextureManager&&) = delete;

        /// Initialise with a render device — creates fallback textures.
        bool Initialise(RenderDevice& device);

        /// Destroy all owned GPU textures and samplers.
        void Shutdown();

        // ── Texture Loading ──────────────────────────────────

        /**
         * @brief Load a texture asset and upload to GPU, or return cached handle.
         *
         * On cache miss: loads the TextureAsset via AssetService, creates a GPU
         * texture, uploads pixel data, and caches the handle. Returns the fallback
         * texture on failure.
         */
        GPUTextureHandle GetOrLoad(const AssetId& assetId, AssetService& assetService);

        // ── Built-in Textures ────────────────────────────────

        /// 8x8 pink-black checkerboard for missing textures.
        GPUTextureHandle GetFallback() const { return m_fallbackTexture; }

        /// 1x1 white (RGBA 255,255,255,255).
        GPUTextureHandle GetWhite() const { return m_whiteTexture; }

        /// 1x1 black (RGBA 0,0,0,255).
        GPUTextureHandle GetBlack() const { return m_blackTexture; }

        /// 1x1 flat normal (RGBA 128,128,255,255 — tangent-space up).
        GPUTextureHandle GetFlatNormal() const { return m_flatNormalTexture; }

        // ── Sampler Cache ────────────────────────────────────

        /// Return an existing sampler matching these params, or create and cache a new one.
        GPUSamplerHandle GetOrCreateSampler(const SamplerCreateDesc& desc);

    private:
        GPUTextureHandle CreateAndUpload(const TextureAsset& asset);
        GPUTextureHandle CreateSolidColour(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
        GPUTextureHandle CreateCheckerboard();

        static uint64_t HashSamplerDesc(const SamplerCreateDesc& desc);

        RenderDevice* m_device = nullptr;

        // Asset texture cache: AssetId → GPU handle
        std::unordered_map<AssetId, GPUTextureHandle> m_textureCache;

        // Built-in textures
        GPUTextureHandle m_fallbackTexture{};
        GPUTextureHandle m_whiteTexture{};
        GPUTextureHandle m_blackTexture{};
        GPUTextureHandle m_flatNormalTexture{};

        // Sampler deduplication: hash of SamplerCreateDesc → handle
        std::unordered_map<uint64_t, GPUSamplerHandle> m_samplerCache;
    };

} // namespace Wayfinder
