#include "RaylibRenderAPI.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

namespace Wayfinder
{
    std::unique_ptr<IRenderAPI> IRenderAPI::Create(RenderBackend backend)
    {
        switch (backend)
        {
        case RenderBackend::Raylib:
            return std::make_unique<RaylibRenderAPI>();
        }

        return nullptr;
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

    void RaylibRenderAPI::DrawBox(const Matrix4& transform, const Float3& size, const Color& color)
    {
        const ::Matrix nativeTransform = ConvertMatrix(transform);
        const float16 matrixValues = MatrixToFloatV(nativeTransform);

        rlPushMatrix();
        rlMultMatrixf(matrixValues.v);
        ::DrawCube({ 0.0f, 0.0f, 0.0f }, size.x, size.y, size.z, ConvertColor(color));
        rlPopMatrix();
    }

    void RaylibRenderAPI::DrawBoxWires(const Matrix4& transform, const Float3& size, const Color& color)
    {
        const ::Matrix nativeTransform = ConvertMatrix(transform);
        const float16 matrixValues = MatrixToFloatV(nativeTransform);

        rlPushMatrix();
        rlMultMatrixf(matrixValues.v);
        ::DrawCubeWires({ 0.0f, 0.0f, 0.0f }, size.x, size.y, size.z, ConvertColor(color));
        rlPopMatrix();
    }

    void RaylibRenderAPI::DrawLine3D(const Float3& start, const Float3& end, const Color& color)
    {
        ::DrawLine3D({start.x, start.y, start.z}, {end.x, end.y, end.z}, ConvertColor(color));
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

    ::Matrix RaylibRenderAPI::ConvertMatrix(const Matrix4& matrix)
    {
        return {
            matrix.m0,
            matrix.m4,
            matrix.m8,
            matrix.m12,
            matrix.m1,
            matrix.m5,
            matrix.m9,
            matrix.m13,
            matrix.m2,
            matrix.m6,
            matrix.m10,
            matrix.m14,
            matrix.m3,
            matrix.m7,
            matrix.m11,
            matrix.m15,
        };
    }
}
