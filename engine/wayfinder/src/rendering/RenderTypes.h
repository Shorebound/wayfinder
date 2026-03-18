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
        // Stage 6: RenderTargetHandle for off-screen targets
    };

    struct RenderPassDescriptor
    {
        std::string debugName;
        ColorAttachmentDescriptor colorAttachment{};
        bool targetSwapchain = true;
        // Stage 6: DepthAttachmentDescriptor, multiple color attachments
    };

    // ── Device Info ──────────────────────────────────────────

    struct RenderDeviceInfo
    {
        std::string BackendName = "Unknown";
        std::string DeviceName;
        std::string DriverInfo;
    };

} // namespace Wayfinder
