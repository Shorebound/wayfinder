#include "RaylibRenderAPI.h"
#include "raylib.h"

namespace Wayfinder
{
    std::unique_ptr<IRenderAPI> IRenderAPI::Create()
    {
        return std::make_unique<RaylibRenderAPI>();
    }

    void RaylibRenderAPI::Initialize()
    {
        // Raylib initialization is handled by RaylibWindow
    }

    void RaylibRenderAPI::Shutdown()
    {
        // Raylib shutdown is handled by RaylibWindow
    }

    void RaylibRenderAPI::DrawText(const std::string& text, int x, int y, int fontSize, const Color& color)
    {
        ::DrawText(text.c_str(), x, y, fontSize, ConvertColor(color));
    }

    void RaylibRenderAPI::DrawFPS(int x, int y)
    {
        ::DrawFPS(x, y);
    }

    void RaylibRenderAPI::Begin3DMode(const Camera& camera)
    {
        ::BeginMode3D(ConvertCamera(camera));
    }

    void RaylibRenderAPI::End3DMode()
    {
        ::EndMode3D();
    }

    void RaylibRenderAPI::DrawGrid(int slices, float spacing)
    {
        ::DrawGrid(slices, spacing);
    }

    void RaylibRenderAPI::DrawCube(float x, float y, float z, float width, float height, float depth, const Color& color)
    {
        ::DrawCube({ x, y, z }, width, height, depth, ConvertColor(color));
    }

    void RaylibRenderAPI::DrawCubeWires(float x, float y, float z, float width, float height, float depth, const Color& color)
    {
        ::DrawCubeWires({ x, y, z }, width, height, depth, ConvertColor(color));
    }

    ::Color RaylibRenderAPI::ConvertColor(const Color& color)
    {
        return CLITERAL(::Color){ color.r, color.g, color.b, color.a };
    }

    ::Camera3D RaylibRenderAPI::ConvertCamera(const Camera& camera)
    {
        ::Camera3D result;
        result.position = { camera.Position.x, camera.Position.y, camera.Position.z };
        result.target = { camera.Target.x, camera.Target.y, camera.Target.z };
        result.up = { camera.Up.x, camera.Up.y, camera.Up.z };
        result.fovy = camera.FOV;
        result.projection = camera.ProjectionType;
        return result;
    }
}
