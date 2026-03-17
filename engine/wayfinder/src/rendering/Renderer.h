#pragma once
#include "RenderAPI.h"

#include <memory>

namespace Wayfinder
{
    class AssetService;
    class IGraphicsContext;
    class IRenderAPI;
    struct RenderFrame;
    class RenderPipeline;
    class RenderResourceCache;

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
        void SetRenderInterfaces(IGraphicsContext& graphicsContext, IRenderAPI& renderAPI);
        void SetAssetService(const std::shared_ptr<AssetService>& assetService);

    private:
        Color m_clearColor;
        std::shared_ptr<AssetService> m_assetService;
        IGraphicsContext* m_graphicsContext = nullptr;
        IRenderAPI* m_renderAPI = nullptr;
        std::unique_ptr<RenderPipeline> m_renderPipeline;
        std::unique_ptr<RenderResourceCache> m_renderResources;

        int m_screenWidth;
        int m_screenHeight;

        bool m_isInitialized;
    };

} // namespace Wayfinder
