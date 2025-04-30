#pragma once
#include "../RenderAPI.h"

namespace Wayfinder
{
    class RaylibRenderAPI : public IRenderAPI
    {
    public:
        RaylibRenderAPI() = default;
        virtual ~RaylibRenderAPI() = default;

        void Initialize() override;
        void Shutdown() override;
        
        // 2D Rendering
        void DrawText(const std::string& text, int x, int y, int fontSize, const Color& color) override;
        void DrawFPS(int x, int y) override;
        
        // 3D Rendering
        void Begin3DMode(const Camera& camera) override;
        void End3DMode() override;
        void DrawGrid(int slices, float spacing) override;
        void DrawCube(float x, float y, float z, float width, float height, float depth, const Color& color) override;
        void DrawCubeWires(float x, float y, float z, float width, float height, float depth, const Color& color) override;
        
    private:
        ::Color ConvertColor(const Color& color);
        ::Camera3D ConvertCamera(const Camera& camera);
    };
}
