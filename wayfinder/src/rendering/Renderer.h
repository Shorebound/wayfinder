#pragma once

#include "raylib.h"
#include <memory>

namespace Wayfinder
{
    class Scene;

    class IRenderer
    {
    public:
        virtual void Render(const Scene &scene) = 0;
    };

    class Renderer : public IRenderer
    {
    public:
        Renderer();
        ~Renderer();

        bool Initialize(int screenWidth, int screenHeight);
        void Shutdown();

        void Render(const Scene &scene);

        void SetCameraPosition(float x, float y, float z);
        void SetCameraTarget(float x, float y, float z);

    private:
        void BeginRenderFrame();
        void EndRenderFrame();
        void RenderEntities(const Scene &scene);

        Camera3D m_camera;

        int m_screenWidth;
        int m_screenHeight;

        bool m_isInitialized;
    };

} // namespace Wayfinder
