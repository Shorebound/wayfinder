#pragma once

#include "RenderFeature.h"
#include "RenderTypes.h"
#include "ShaderManager.h"
#include "ShaderProgram.h"
#include "GPUPipeline.h"
#include "Mesh.h"
#include "PipelineCache.h"
#include "TransientBufferAllocator.h"
#include "TransientResourcePool.h"
#include "RenderGraph.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

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

        // ── RenderFeature API ────────────────────────────────
        void AddFeature(std::unique_ptr<RenderFeature> feature);

        template <typename T>
        void RemoveFeature()
        {
            auto it = std::find_if(m_features.begin(), m_features.end(),
                [](const std::unique_ptr<RenderFeature>& f) { return dynamic_cast<T*>(f.get()) != nullptr; });
            if (it != m_features.end())
            {
                if (m_device) { auto ctx = MakeFeatureContext(); (*it)->OnDetach(ctx); }
                m_features.erase(it);
            }
        }

        template <typename T>
        const T* GetFeature() const
        {
            for (const auto& f : m_features)
            {
                if (auto* ptr = dynamic_cast<const T*>(f.get())) return ptr;
            }
            return nullptr;
        }

        template <typename T>
        T* GetFeature()
        {
            for (auto& f : m_features)
            {
                if (auto* ptr = dynamic_cast<T*>(f.get())) return ptr;
            }
            return nullptr;
        }

    private:
        std::shared_ptr<AssetService> m_assetService;
        RenderDevice* m_device = nullptr;
        std::unique_ptr<RenderPipeline> m_renderPipeline;
        std::unique_ptr<RenderResourceCache> m_renderResources;

        // Extracts the primary directional light from the frame into the scene globals UBO.
        SceneGlobalsUBO BuildSceneGlobals(const RenderFrame& frame) const;

        // Builds the context struct that features receive on attach/detach.
        RenderFeatureContext MakeFeatureContext();

        // ── Shader / Pipeline infrastructure ─────────────────
        ShaderManager m_shaderManager;
        PipelineCache m_pipelineCache;
        ShaderProgramRegistry m_programRegistry;
        TransientBufferAllocator m_transientAllocator;

        // ── Render Graph resources ───────────────────────────
        TransientResourcePool m_transientPool;
        GPUSamplerHandle m_nearestSampler = nullptr;

        // ── Features ─────────────────────────────────────────
        std::vector<std::unique_ptr<RenderFeature>> m_features;

        // ── Debug-only pipeline (PosColor, uses debug_unlit shaders) ──
        GPUPipeline m_debugLinePipeline;

        // Single built-in mesh — all scene primitives use PosNormalColor
        Mesh m_primitiveMesh;

        int m_screenWidth;
        int m_screenHeight;

        bool m_isInitialized;
    };

} // namespace Wayfinder
