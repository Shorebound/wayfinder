#pragma once

#include "RenderTypes.h"
#include "ShaderManager.h"
#include "ShaderProgram.h"
#include "GPUPipeline.h"
#include "Mesh.h"
#include "PipelineCache.h"
#include "TransientBufferAllocator.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace Wayfinder
{
    class AssetService;
    class RenderDevice;
    struct EngineConfig;
    struct RenderFrame;
    struct RenderLightSubmission;
    class RenderPipeline;
    class RenderResourceCache;

    // Per-frame scene globals pushed to fragment UBO slot 1 for shaders that need it.
    struct SceneGlobalsUBO
    {
        Float3 LightDirection{0.0f, -0.7f, -0.5f};
        float LightIntensity = 1.0f;
        Float3 LightColor{1.0f, 1.0f, 1.0f};
        float Ambient = 0.15f;
    };

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

        // Extracts the primary directional light from the frame into the scene globals UBO.
        SceneGlobalsUBO BuildSceneGlobals(const RenderFrame& frame) const;

        ShaderManager m_shaderManager;
        PipelineCache m_pipelineCache;
        ShaderProgramRegistry m_programRegistry;
        TransientBufferAllocator m_transientAllocator;

        // Debug-only pipeline (PosColor, uses debug_unlit shaders)
        GPUPipeline m_debugLinePipeline;

        // Single built-in mesh — all scene primitives use PosNormalColor
        Mesh m_primitiveMesh;

        int m_screenWidth;
        int m_screenHeight;

        bool m_isInitialized;
    };

} // namespace Wayfinder
