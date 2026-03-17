#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "wayfinder_exports.h"

namespace Wayfinder
{
    // Forward declarations
    class Scene;
    class Entity;
    
    // Camera abstraction
    struct Camera
    {
        struct Vec3 
        {
            float x, y, z;
        };
        
        Vec3 Position;
        Vec3 Target;
        Vec3 Up;
        float FOV;
        int ProjectionType; // 0 = Perspective, 1 = Orthographic
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
        
        // 2D Rendering
        virtual void DrawText(const std::string& text, int x, int y, int fontSize, const Color& color) = 0;
        virtual void DrawFPS(int x, int y) = 0;
        
        // 3D Rendering
        virtual void Begin3DMode(const Camera& camera) = 0;
        virtual void End3DMode() = 0;
        virtual void DrawGrid(int slices, float spacing) = 0;
        virtual void DrawCube(float x, float y, float z, float width, float height, float depth, const Color& color) = 0;
        virtual void DrawCubeWires(float x, float y, float z, float width, float height, float depth, const Color& color) = 0;
        
        static std::unique_ptr<IRenderAPI> Create();
    };
}
