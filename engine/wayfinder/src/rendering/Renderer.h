#pragma once

#include "RenderTypes.h"

#include <memory>

namespace Wayfinder
{
    class AssetService;
    class RenderDevice;
    struct RenderFrame;
    class RenderPipeline;
    class RenderResourceCache;

    class WAYFINDER_API Renderer
    {
    public:
        Renderer();
        ~Renderer();

        bool Initialize(RenderDevice& device, int screenWidth, int screenHeight);
        void Shutdown();

        void Render(const RenderFrame& frame);
        void SetAssetService(const std::shared_ptr<AssetService>& assetService);

    private:
        std::shared_ptr<AssetService> m_assetService;
        RenderDevice* m_device = nullptr;
        std::unique_ptr<RenderPipeline> m_renderPipeline;
        std::unique_ptr<RenderResourceCache> m_renderResources;

        int m_screenWidth;
        int m_screenHeight;

        bool m_isInitialized;
    };

} // namespace Wayfinder
