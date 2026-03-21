#include "TextureManager.h"

#include "rendering/backend/RenderDevice.h"
#include "core/Log.h"

#include <array>
#include <cstdint>

namespace Wayfinder
{
    bool TextureManager::Initialise(RenderDevice& device)
    {
        m_device = &device;

        m_whiteTexture = CreateSolidColour(255, 255, 255, 255);
        m_blackTexture = CreateSolidColour(0, 0, 0, 255);
        m_flatNormalTexture = CreateSolidColour(128, 128, 255, 255);
        m_fallbackTexture = CreateCheckerboard();

        if (!m_whiteTexture || !m_blackTexture || !m_flatNormalTexture || !m_fallbackTexture)
        {
            WAYFINDER_ERROR(LogRenderer, "TextureManager: Failed to create one or more built-in textures");
            return false;
        }

        WAYFINDER_INFO(LogRenderer, "TextureManager initialised (4 built-in textures)");
        return true;
    }

    void TextureManager::Shutdown()
    {
        if (!m_device) return;

        // Destroy cached asset textures
        for (auto& [id, handle] : m_textureCache)
        {
            if (handle) m_device->DestroyTexture(handle);
        }
        m_textureCache.clear();

        // Destroy built-in textures
        if (m_fallbackTexture)    { m_device->DestroyTexture(m_fallbackTexture);    m_fallbackTexture = {}; }
        if (m_whiteTexture)       { m_device->DestroyTexture(m_whiteTexture);       m_whiteTexture = {}; }
        if (m_blackTexture)       { m_device->DestroyTexture(m_blackTexture);       m_blackTexture = {}; }
        if (m_flatNormalTexture)  { m_device->DestroyTexture(m_flatNormalTexture);  m_flatNormalTexture = {}; }

        // Destroy cached samplers
        for (auto& [hash, handle] : m_samplerCache)
        {
            if (handle) m_device->DestroySampler(handle);
        }
        m_samplerCache.clear();

        m_device = nullptr;
    }

    GPUTextureHandle TextureManager::GetOrLoad(const AssetId& assetId, AssetService& assetService)
    {
        // Cache hit
        if (const auto it = m_textureCache.find(assetId); it != m_textureCache.end())
        {
            return it->second;
        }

        // Load TextureAsset from the asset system
        std::string error;
        const TextureAsset* asset = assetService.LoadAsset<TextureAsset>(assetId, error);
        if (!asset)
        {
            WAYFINDER_WARNING(LogRenderer, "TextureManager: Failed to load texture asset '{}': {}",
                assetId.ToString(), error);
            m_textureCache[assetId] = m_fallbackTexture;
            return m_fallbackTexture;
        }

        // Upload to GPU
        GPUTextureHandle gpuTexture = CreateAndUpload(*asset);
        if (!gpuTexture)
        {
            WAYFINDER_WARNING(LogRenderer, "TextureManager: GPU upload failed for texture '{}', using fallback",
                asset->Name);
            m_textureCache[assetId] = m_fallbackTexture;
            return m_fallbackTexture;
        }

        m_textureCache[assetId] = gpuTexture;
        WAYFINDER_INFO(LogRenderer, "TextureManager: Loaded '{}' ({}x{}) to GPU",
            asset->Name, asset->Width, asset->Height);
        return gpuTexture;
    }

    GPUSamplerHandle TextureManager::GetOrCreateSampler(const SamplerCreateDesc& desc)
    {
        const uint64_t hash = HashSamplerDesc(desc);

        if (const auto it = m_samplerCache.find(hash); it != m_samplerCache.end())
        {
            return it->second;
        }

        GPUSamplerHandle sampler = m_device->CreateSampler(desc);
        if (sampler)
        {
            m_samplerCache[hash] = sampler;
        }
        return sampler;
    }

    GPUTextureHandle TextureManager::CreateAndUpload(const TextureAsset& asset)
    {
        if (asset.Width == 0 || asset.Height == 0 || asset.PixelData.empty())
        {
            return GPUTextureHandle::Invalid();
        }

        TextureCreateDesc desc;
        desc.width = asset.Width;
        desc.height = asset.Height;
        desc.format = TextureFormat::RGBA8_UNORM;
        desc.usage = TextureUsage::Sampler;

        GPUTextureHandle texture = m_device->CreateTexture(desc);
        if (!texture) return GPUTextureHandle::Invalid();

        m_device->UploadToTexture(
            texture,
            asset.PixelData.data(),
            asset.Width,
            asset.Height,
            asset.Width * asset.Channels);

        return texture;
    }

    GPUTextureHandle TextureManager::CreateSolidColour(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        TextureCreateDesc desc;
        desc.width = 1;
        desc.height = 1;
        desc.format = TextureFormat::RGBA8_UNORM;
        desc.usage = TextureUsage::Sampler;

        GPUTextureHandle texture = m_device->CreateTexture(desc);
        if (!texture) return GPUTextureHandle::Invalid();

        const std::array<uint8_t, 4> pixel = {r, g, b, a};
        m_device->UploadToTexture(texture, pixel.data(), 1, 1, 4);

        return texture;
    }

    GPUTextureHandle TextureManager::CreateCheckerboard()
    {
        constexpr uint32_t kSize = 8;
        constexpr uint32_t kChannels = 4;
        std::array<uint8_t, kSize * kSize * kChannels> pixels{};

        for (uint32_t y = 0; y < kSize; ++y)
        {
            for (uint32_t x = 0; x < kSize; ++x)
            {
                const bool isLight = ((x + y) % 2) == 0;
                const size_t offset = (y * kSize + x) * kChannels;
                pixels[offset + 0] = isLight ? 255 : 0;   // R: pink or black
                pixels[offset + 1] = isLight ? 0   : 0;   // G
                pixels[offset + 2] = isLight ? 200 : 0;    // B
                pixels[offset + 3] = 255;                   // A
            }
        }

        TextureCreateDesc desc;
        desc.width = kSize;
        desc.height = kSize;
        desc.format = TextureFormat::RGBA8_UNORM;
        desc.usage = TextureUsage::Sampler;

        GPUTextureHandle texture = m_device->CreateTexture(desc);
        if (!texture) return GPUTextureHandle::Invalid();

        m_device->UploadToTexture(texture, pixels.data(), kSize, kSize, kSize * kChannels);

        return texture;
    }

    uint64_t TextureManager::HashSamplerDesc(const SamplerCreateDesc& desc)
    {
        uint64_t hash = 0;
        hash |= static_cast<uint64_t>(desc.minFilter) << 0;
        hash |= static_cast<uint64_t>(desc.magFilter) << 8;
        hash |= static_cast<uint64_t>(desc.addressModeU) << 16;
        hash |= static_cast<uint64_t>(desc.addressModeV) << 24;
        return hash;
    }

} // namespace Wayfinder
