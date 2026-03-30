#include "TextureManager.h"

#include "core/Log.h"
#include "rendering/backend/RenderDevice.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <span>

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
            Log::Error(LogRenderer, "TextureManager: Failed to create one or more built-in textures");
            Shutdown();
            return false;
        }

        Log::Info(LogRenderer, "TextureManager initialised (4 built-in textures)");
        return true;
    }

    void TextureManager::Shutdown()
    {
        if (!m_device)
        {
            return;
        }

        // Destroy cached asset textures (skip built-ins — they are destroyed below)
        for (auto& [id, handle] : m_textureCache)
        {
            if (handle && handle != m_fallbackTexture && handle != m_whiteTexture && handle != m_blackTexture && handle != m_flatNormalTexture)
            {
                m_device->DestroyTexture(handle);
            }
        }
        m_textureCache.clear();

        // Destroy built-in textures
        if (m_fallbackTexture)
        {
            m_device->DestroyTexture(m_fallbackTexture);
            m_fallbackTexture = {};
        }
        if (m_whiteTexture)
        {
            m_device->DestroyTexture(m_whiteTexture);
            m_whiteTexture = {};
        }
        if (m_blackTexture)
        {
            m_device->DestroyTexture(m_blackTexture);
            m_blackTexture = {};
        }
        if (m_flatNormalTexture)
        {
            m_device->DestroyTexture(m_flatNormalTexture);
            m_flatNormalTexture = {};
        }

        // Destroy cached samplers
        for (auto& [hash, handle] : m_samplerCache)
        {
            if (handle)
            {
                m_device->DestroySampler(handle);
            }
        }
        m_samplerCache.clear();

        m_device = nullptr;
    }

    GPUTextureHandle TextureManager::GetOrLoad(const AssetId& assetId, AssetService& assetService)
    {
        if (!m_device)
        {
            Log::Warn(LogRenderer, "TextureManager::GetOrLoad called without a valid device");
            return m_fallbackTexture;
        }

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
            Log::Warn(LogRenderer, "TextureManager: Failed to load texture asset '{}': {}", assetId.ToString(), error);
            m_textureCache[assetId] = m_fallbackTexture;
            return m_fallbackTexture;
        }

        // If pixel data was previously released (e.g. after device reinit), invalidate and reload from disk
        if (asset->PixelData.empty())
        {
            assetService.InvalidateTextureAsset(assetId);
            asset = assetService.LoadAsset<TextureAsset>(assetId, error);
            if (!asset || asset->PixelData.empty())
            {
                Log::Warn(LogRenderer, "TextureManager: Failed to reload pixel data for texture '{}': {}", assetId.ToString(), error);
                m_textureCache[assetId] = m_fallbackTexture;
                return m_fallbackTexture;
            }
        }

        // Upload to GPU
        GPUTextureHandle gpuTexture = CreateAndUpload(*asset);
        if (!gpuTexture)
        {
            Log::Warn(LogRenderer, "TextureManager: GPU upload failed for texture '{}', using fallback", asset->Name);
            m_textureCache[assetId] = m_fallbackTexture;
            return m_fallbackTexture;
        }

        m_textureCache[assetId] = gpuTexture;

        Log::Info(LogRenderer, "TextureManager: Loaded '{}' ({}x{}, mipLevels={}) to GPU", asset->Name, asset->Width, asset->Height, asset->MipLevels);

        // Release CPU-side pixel data now that it's on the GPU
        assetService.ReleaseTexturePixelData(assetId);

        return gpuTexture;
    }

    GPUSamplerHandle TextureManager::GetOrCreateSampler(const SamplerCreateDesc& desc)
    {
        if (!m_device)
        {
            Log::Warn(LogRenderer, "TextureManager::GetOrCreateSampler called without a valid device");
            return {};
        }

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
        desc.Width = asset.Width;
        desc.Height = asset.Height;
        desc.Format = TextureFormat::RGBA8_UNORM;
        desc.Usage = TextureUsage::Sampler;
        desc.MipLevels = asset.MipLevels; // 0 = auto, device resolves

        GPUTextureHandle texture = m_device->CreateTexture(desc);
        if (!texture)
        {
            return GPUTextureHandle::Invalid();
        }

        m_device->UploadToTexture(texture, asset.PixelData.data(), asset.Width, asset.Height, asset.Width * asset.Channels);

        if (asset.MipLevels != 1)
        {
            m_device->GenerateMipmaps(texture);
        }

        return texture;
    }

    GPUTextureHandle TextureManager::CreateSolidColour(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        TextureCreateDesc desc;
        desc.Width = 1;
        desc.Height = 1;
        desc.Format = TextureFormat::RGBA8_UNORM;
        desc.Usage = TextureUsage::Sampler;

        GPUTextureHandle texture = m_device->CreateTexture(desc);
        if (!texture)
        {
            return GPUTextureHandle::Invalid();
        }

        const std::array<uint8_t, 4> pixel = {r, g, b, a};
        m_device->UploadToTexture(texture, pixel.data(), 1, 1, 4);

        return texture;
    }

    GPUTextureHandle TextureManager::CreateCheckerboard()
    {
        constexpr uint32_t K_SIZE = 8;
        constexpr uint32_t K_CHANNELS = 4;
        constexpr size_t K_PIXEL_COUNT = static_cast<size_t>(K_SIZE) * static_cast<size_t>(K_SIZE);
        constexpr size_t K_BUFFER_SIZE = K_PIXEL_COUNT * static_cast<size_t>(K_CHANNELS);
        std::array<uint8_t, K_BUFFER_SIZE> pixels{};
        const std::span<uint8_t, K_BUFFER_SIZE> pixelSpan{pixels};

        for (uint32_t y = 0; y < K_SIZE; ++y)
        {
            for (uint32_t x = 0; x < K_SIZE; ++x)
            {
                const bool isLight = ((x + y) % 2) == 0;
                const size_t offset = ((static_cast<size_t>(y) * static_cast<size_t>(K_SIZE)) + static_cast<size_t>(x)) * static_cast<size_t>(K_CHANNELS);
                auto texel = pixelSpan.subspan(offset, K_CHANNELS);
                const std::array<uint8_t, 4> texelValues =
                {
                    isLight ? static_cast<uint8_t>(255) : static_cast<uint8_t>(0),
                    0,
                    isLight ? static_cast<uint8_t>(200) : static_cast<uint8_t>(0),
                    255,
                };
                std::ranges::copy(texelValues, texel.begin());
            }
        }

        TextureCreateDesc desc;
        desc.Width = K_SIZE;
        desc.Height = K_SIZE;
        desc.Format = TextureFormat::RGBA8_UNORM;
        desc.Usage = TextureUsage::Sampler;

        GPUTextureHandle texture = m_device->CreateTexture(desc);
        if (!texture)
        {
            return GPUTextureHandle::Invalid();
        }

        m_device->UploadToTexture(texture, pixels.data(), K_SIZE, K_SIZE, K_SIZE * K_CHANNELS);

        return texture;
    }

    uint64_t TextureManager::HashSamplerDesc(const SamplerCreateDesc& desc)
    {
        static_assert(sizeof(SamplerFilter) == 1, "SamplerFilter must be 1 byte for hash packing");
        static_assert(sizeof(SamplerAddressMode) == 1, "SamplerAddressMode must be 1 byte for hash packing");
        static_assert(sizeof(SamplerMipmapMode) == 1, "SamplerMipmapMode must be 1 byte for hash packing");

        // FNV-1a 64-bit hash over all fields
        uint64_t hash = 14695981039346656037ull;

        auto feedByte = [&hash](uint8_t byte)
        {
            hash ^= static_cast<uint64_t>(byte);
            hash *= 1099511628211ull;
        };

        auto feedFloat = [&feedByte](float value)
        {
            uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            for (int i = 0; i < 4; ++i)
            {
                feedByte(static_cast<uint8_t>((bits >> (i * 8)) & 0xFF));
            }
        };

        feedByte(static_cast<uint8_t>(desc.MinFilter));
        feedByte(static_cast<uint8_t>(desc.MagFilter));
        feedByte(static_cast<uint8_t>(desc.AddressModeU));
        feedByte(static_cast<uint8_t>(desc.AddressModeV));
        feedByte(static_cast<uint8_t>(desc.MipmapMode));
        feedFloat(desc.MinLod);
        feedFloat(desc.MaxLod);
        feedFloat(desc.MipLodBias);
        feedByte(desc.EnableAnisotropy ? 1 : 0);
        feedFloat(desc.MaxAnisotropy);

        return hash;
    }

} // namespace Wayfinder
