#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

#include "../core/BackendConfig.h"
#include "GPUHandles.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{

    // ── Engine Math Aliases ──────────────────────────────────

    using Float3 = glm::vec3;
    using Float4 = glm::vec4;
    using Matrix4 = glm::mat4;

    // ── Color ────────────────────────────────────────────────

    struct Color
    {
        uint8_t r, g, b, a;

        static Color White() { return {.r = 255, .g = 255, .b = 255, .a = 255}; }
        static Color Black() { return {.r = 0, .g = 0, .b = 0, .a = 255}; }
        static Color Red() { return {.r = 255, .g = 0, .b = 0, .a = 255}; }
        static Color Green() { return {.r = 0, .g = 255, .b = 0, .a = 255}; }
        static Color Blue() { return {.r = 0, .g = 0, .b = 255, .a = 255}; }
        static Color Yellow() { return {.r = 255, .g = 255, .b = 0, .a = 255}; }
        static Color Gray() { return {.r = 128, .g = 128, .b = 128, .a = 255}; }
        static Color DarkGray() { return {.r = 80, .g = 80, .b = 80, .a = 255}; }
    };

    // ── Linear Color ─────────────────────────────────────────
    // Float4 color in linear space for GPU-side work.
    // Conversions from authored sRGB Color happen once at load/extract time.

    struct LinearColor
    {
        float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;

        static LinearColor White() { return {1.0f, 1.0f, 1.0f, 1.0f}; }
        static LinearColor Black() { return {0.0f, 0.0f, 0.0f, 1.0f}; }

        static LinearColor FromColor(const Color& c)
        {
            return {
                .r = static_cast<float>(c.r) / 255.0f,
                .g = static_cast<float>(c.g) / 255.0f,
                .b = static_cast<float>(c.b) / 255.0f,
                .a = static_cast<float>(c.a) / 255.0f,
            };
        }

        glm::vec4 ToVec4() const { return {r, g, b, a}; }
        Float3 ToFloat3() const { return {r, g, b}; }
    };

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
        Sampler      = 1u << 0,
        ColourTarget = 1u << 1,
        DepthTarget  = 1u << 2,
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
    };

    // ── Sampler ───────────────────────────────────────────────

    enum class SamplerFilter : uint8_t
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

        static ClearValue FromColor(const Color& c)
        {
            return {
                .r = c.r / 255.0f,
                .g = c.g / 255.0f,
                .b = c.b / 255.0f,
                .a = c.a / 255.0f,
            };
        }
    };

    // ── Render Pass Descriptor ───────────────────────────────

    struct ColorAttachmentDescriptor
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
        std::string debugName;
        ColorAttachmentDescriptor colorAttachment{};
        DepthAttachmentDescriptor depthAttachment{};
        bool targetSwapchain = true;
        GPUTextureHandle colorTarget{};  // If set and !targetSwapchain, render to this texture
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
