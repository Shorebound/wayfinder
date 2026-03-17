#pragma once
#include "RenderAPI.h"

namespace Wayfinder
{
    struct RenderFrame;
}

namespace Wayfinder
{
    class IRenderAPI;
    class IGraphicsContext;

    class WAYFINDER_API Renderer
    {
    public:
        Renderer();
        ~Renderer();

        virtual bool Initialize(int screenWidth, int screenHeight);
        virtual void Shutdown();

        virtual void BeginFrame();
        virtual void Render(const RenderFrame& frame);
        virtual void EndFrame();

        void SetCameraPosition(float x, float y, float z);
        void SetCameraTarget(float x, float y, float z);

    private:
        void RenderMeshes(const RenderFrame& frame);
        void RenderDebugPrimitives(const RenderFrame& frame);
        Camera m_camera;
        Color m_clearColor;

        int m_screenWidth;
        int m_screenHeight;

        bool m_isInitialized;
    };

} // namespace Wayfinder
