#pragma once

#include "RenderTypes.h"
#include "ShaderManager.h"
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

        // Returns the GPUPipeline for a given shader name, or nullptr if not found.
        GPUPipeline* GetPipelineForShader(const std::string& shaderName);
        // Returns the appropriate mesh for a given shader name.
        Mesh* GetMeshForShader(const std::string& shaderName);
        // Extracts the primary directional light from the frame. Returns a default if none found.
        void ExtractPrimaryLight(const RenderFrame& frame, Float3& direction, float& intensity, Float3& color) const;

        ShaderManager m_shaderManager;
        PipelineCache m_pipelineCache;
        TransientBufferAllocator m_transientAllocator;

        // Named pipelines — keyed by shader name
        std::unordered_map<std::string, GPUPipeline> m_pipelines;
        GPUPipeline m_debugLinePipeline;

        // Built-in meshes
        Mesh m_cubeMesh;         // PosColor — for unlit
        Mesh m_litCubeMesh;      // PosNormalColor — for basic_lit

        int m_screenWidth;
        int m_screenHeight;

        bool m_isInitialized;
    };

} // namespace Wayfinder
