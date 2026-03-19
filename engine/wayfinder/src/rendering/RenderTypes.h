#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

#include "../core/BackendConfig.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    // ── Engine Math Aliases ──────────────────────────────────

    using Float3 = glm::vec3;
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
    };

    // ── Device Info ──────────────────────────────────────────

    struct RenderDeviceInfo
    {
        std::string BackendName = "Unknown";
        std::string DeviceName;
        std::string DriverInfo;
    };

} // namespace Wayfinder
