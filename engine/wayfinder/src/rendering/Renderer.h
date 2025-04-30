#pragma once
#include "RenderAPI.h"

namespace Wayfinder
{
    class Scene;
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
        virtual void Render(const Scene& scene);
        virtual void EndFrame();

        void SetCameraPosition(float x, float y, float z);
        void SetCameraTarget(float x, float y, float z);

    private:
        void RenderEntities(const Scene& scene);
        Camera m_camera;

        int m_screenWidth;
        int m_screenHeight;

        bool m_isInitialized;
    };

} // namespace Wayfinder
