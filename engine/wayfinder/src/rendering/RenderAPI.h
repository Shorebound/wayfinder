#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "../core/BackendConfig.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    struct RenderBackendCapabilities
    {
        std::string BackendName = "Unknown";
        size_t MaxViewCount = 1;
        bool SupportsScenePasses = true;
        bool SupportsDebugPasses = true;
        bool SupportsRenderTargets = false;
        bool SupportsBoxGeometry = true;
        bool SupportsDebugLines = true;
    };

    struct Float3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct Matrix4
    {
        float m0 = 1.0f;
        float m4 = 0.0f;
        float m8 = 0.0f;
        float m12 = 0.0f;
        float m1 = 0.0f;
        float m5 = 1.0f;
        float m9 = 0.0f;
        float m13 = 0.0f;
        float m2 = 0.0f;
        float m6 = 0.0f;
        float m10 = 1.0f;
        float m14 = 0.0f;
        float m3 = 0.0f;
        float m7 = 0.0f;
        float m11 = 0.0f;
        float m15 = 1.0f;

        static Matrix4 Identity()
        {
            return {};
        }
    };

    struct Camera
    {
        Float3 Position{};
        Float3 Target{};
        Float3 Up{0.0f, 1.0f, 0.0f};
        float FOV = 45.0f;
        int ProjectionType = 0; // 0 = Perspective, 1 = Orthographic
    };
    
    // Color abstraction
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
    
    // Interface for rendering API
    class WAYFINDER_API IRenderAPI
    {
    public:
        virtual ~IRenderAPI() = default;
        
        virtual void Initialize() = 0;
        virtual void Shutdown() = 0;
        virtual const RenderBackendCapabilities& GetCapabilities() const = 0;
        
        // 2D Rendering
        virtual void DrawText(const std::string& text, int x, int y, int fontSize, const Color& color) = 0;
        virtual void DrawFPS(int x, int y) = 0;
        
        // 3D Rendering
        virtual void Begin3DMode(const Camera& camera) = 0;
        virtual void End3DMode() = 0;
        virtual void DrawGrid(int slices, float spacing) = 0;
        virtual void DrawBox(const Matrix4& transform, const Float3& size, const Color& color) = 0;
        virtual void DrawBoxWires(const Matrix4& transform, const Float3& size, const Color& color) = 0;
        virtual void DrawLine3D(const Float3& start, const Float3& end, const Color& color) = 0;
        
        static std::unique_ptr<IRenderAPI> Create(RenderBackend backend = RenderBackend::Raylib);
    };
}
