#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "core/Types.h"
#include "platform/BackendConfig.h"
#include "rendering/backend/GPUHandles.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    // ── Camera ───────────────────────────────────────────────

    struct Camera
    {
        Float3 Position{0.0f, 0.0f, 0.0f};
        Float3 Target{0.0f, 0.0f, 0.0f};
        Float3 Up{0.0f, 1.0f, 0.0f};
        float FOV = 45.0f;
        int ProjectionType = 0; // 0 = Perspective, 1 = Orthographic
    };

    // ── Texture ───────────────────────────────────────────────

    enum class TextureFormat : uint8_t
    {
        RGBA8_UNORM,
        BGRA8_UNORM,
        R16_FLOAT,
        RGBA16_FLOAT,
        R32_FLOAT,
        D32_FLOAT,
        D24_UNORM_S8,
    };

    enum class TextureUsage : uint32_t
    {
        None = 0,
        Sampler = 1u << 0,
        ColourTarget = 1u << 1,
        DepthTarget = 1u << 2,
        SamplerColourTarget = Sampler | ColourTarget,
        SamplerDepthTarget = Sampler | DepthTarget,
        ColourTargetDepthTarget = ColourTarget | DepthTarget,
        All = Sampler | ColourTarget | DepthTarget,
    };

    inline TextureUsage operator|(TextureUsage a, TextureUsage b)
    {
        return static_cast<TextureUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline TextureUsage& operator|=(TextureUsage& lhs, TextureUsage rhs)
    {
        lhs = lhs | rhs;
        return lhs;
    }

    inline TextureUsage operator&(TextureUsage a, TextureUsage b)
    {
        return static_cast<TextureUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    inline TextureUsage& operator&=(TextureUsage& lhs, TextureUsage rhs)
    {
        lhs = lhs & rhs;
        return lhs;
    }

    inline bool HasFlag(TextureUsage value, TextureUsage flag)
    {
        return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
    }

    struct TextureCreateDesc
    {
        uint32_t width = 0;
        uint32_t height = 0;
        TextureFormat format = TextureFormat::RGBA8_UNORM;
        TextureUsage usage = TextureUsage::ColourTarget;

        /**
         * @brief Mip level count.
         * 0 = generate full mip chain (floor(log2(max(w,h))) + 1).
         * 1 = base level only (no mipmaps).
         * N = explicit mip level count.
         */
        uint32_t mipLevels = 1;
    };

    /**
     * @brief Calculate the full mip chain level count for a 2D texture.
     * @return floor(log2(max(width, height))) + 1, or 1 if either dimension is zero.
     */
    inline constexpr uint32_t CalculateMipLevels(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
        {
            return 1;
        }
        uint32_t levels = 1;
        uint32_t dim = (width > height) ? width : height;
        while (dim > 1)
        {
            dim >>= 1;
            ++levels;
        }
        return levels;
    }

    struct Extent2D
    {
        uint32_t width = 0;
        uint32_t height = 0;
    };

    // ── Sampler ───────────────────────────────────────────────

    enum class SamplerFilter : uint8_t
    {
        Nearest,
        Linear,
    };

    enum class SamplerMipmapMode : uint8_t
    {
        Nearest,
        Linear,
    };

    enum class SamplerAddressMode : uint8_t
    {
        Repeat,
        ClampToEdge,
        MirroredRepeat,
    };

    struct SamplerCreateDesc
    {
        SamplerFilter minFilter = SamplerFilter::Nearest;
        SamplerFilter magFilter = SamplerFilter::Nearest;
        SamplerAddressMode addressModeU = SamplerAddressMode::ClampToEdge;
        SamplerAddressMode addressModeV = SamplerAddressMode::ClampToEdge;
        SamplerMipmapMode mipmapMode = SamplerMipmapMode::Nearest;

        /** @brief Minimum mip LOD clamp. 0 = use base level. */
        float minLod = 0.0f;

        /** @brief Maximum mip LOD clamp. Large value = allow all mips. */
        float maxLod = 1000.0f;

        /** @brief Bias applied to the computed mip LOD. Negative = sharper. */
        float mipLodBias = 0.0f;

        /** @brief Enable anisotropic filtering. */
        bool enableAnisotropy = false;

        /** @brief Maximum anisotropy level (1–16). Only used when enableAnisotropy is true. */
        float maxAnisotropy = 1.0f;
    };

    // ── GPU Enums ────────────────────────────────────────────

    enum class LoadOp : uint8_t
    {
        Load,
        Clear,
        DontCare,
    };

    enum class StoreOp : uint8_t
    {
        Store,
        DontCare,
    };

    // ── Clear Value ──────────────────────────────────────────

    struct ClearValue
    {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 1.0f;

        static ClearValue FromColour(const Colour& c)
        {
            return {
                .r = static_cast<float>(c.r) / 255.0f,
                .g = static_cast<float>(c.g) / 255.0f,
                .b = static_cast<float>(c.b) / 255.0f,
                .a = static_cast<float>(c.a) / 255.0f,
            };
        }
    };

    // ── Render Pass Descriptor ───────────────────────────────

    struct ColourAttachmentDescriptor
    {
        ClearValue clearValue{};
        LoadOp loadOp = LoadOp::Clear;
        StoreOp storeOp = StoreOp::Store;
    };

    struct DepthAttachmentDescriptor
    {
        float clearDepth = 1.0f;
        LoadOp loadOp = LoadOp::Clear;
        StoreOp storeOp = StoreOp::DontCare;
        bool enabled = false;
    };

    struct RenderPassDescriptor
    {
        std::string_view debugName;
        ColourAttachmentDescriptor colourAttachment{};
        DepthAttachmentDescriptor depthAttachment{};
        bool targetSwapchain = true;
        GPUTextureHandle colourTarget{}; // If set and !targetSwapchain, render to this texture
        GPUTextureHandle depthTarget{};  // If set, use instead of auto-managed depth
    };

    // ── Device Info ──────────────────────────────────────────

    struct RenderDeviceInfo
    {
        std::string BackendName = "Unknown";
        std::string DeviceName;
        std::string DriverInfo;
    };

} // namespace Wayfinder
