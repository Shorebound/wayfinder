#pragma once

#include "RenderTypes.h"
#include "ShaderManager.h"
#include "GPUPipeline.h"
#include "Mesh.h"
#include "PipelineCache.h"
#include "TransientBufferAllocator.h"

#include <memory>

namespace Wayfinder
{
    class AssetService;
    class RenderDevice;
    struct EngineConfig;
    struct RenderFrame;
    class RenderPipeline;
    class RenderResourceCache;

    class WAYFINDER_API Renderer
    {
    public:
        Renderer();
        ~Renderer();

        bool Initialize(RenderDevice& device, const EngineConfig& config);
        void Shutdown();

        void Render(const RenderFrame& frame);
        void SetAssetService(const std::shared_ptr<AssetService>& assetService);

    private:
        std::shared_ptr<AssetService> m_assetService;
        RenderDevice* m_device = nullptr;
        std::unique_ptr<RenderPipeline> m_renderPipeline;
        std::unique_ptr<RenderResourceCache> m_renderResources;

        void RenderDebugPass(const RenderFrame& frame, const Matrix4& view, const Matrix4& projection);

        ShaderManager m_shaderManager;
        GPUPipeline m_unlitPipeline;
        GPUPipeline m_debugLinePipeline;
        Mesh m_cubeMesh;
        PipelineCache m_pipelineCache;
        TransientBufferAllocator m_transientAllocator;

        int m_screenWidth;
        int m_screenHeight;

        bool m_isInitialized;
    };

} // namespace Wayfinder
