#pragma once

#include "RenderFeature.h"
#include "RenderTypes.h"
#include "GPUPipeline.h"
#include "Mesh.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace Wayfinder
{
    class AssetService;
    class RenderContext;
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

        bool Initialise(RenderDevice& device, const EngineConfig& config);
        void Shutdown();

        void Render(const RenderFrame& frame);
        void SetAssetService(const std::shared_ptr<AssetService>& assetService);

        // ── RenderFeature API ────────────────────────────────
        void AddFeature(std::unique_ptr<RenderFeature> feature);

        template <typename T>
        bool RemoveFeature()
        {
            auto it = std::find_if(m_features.begin(), m_features.end(),
                [](const std::unique_ptr<RenderFeature>& f) { return dynamic_cast<T*>(f.get()) != nullptr; });
            if (it != m_features.end())
            {
                if (m_device) { auto ctx = MakeFeatureContext(); (*it)->OnDetach(ctx); }
                m_features.erase(it);
                return true;
            }
            return false;
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
        std::unique_ptr<RenderContext> m_context;
        std::unique_ptr<RenderPipeline> m_renderPipeline;
        std::unique_ptr<RenderResourceCache> m_renderResources;

        // Builds the context struct that features receive on attach/detach.
        RenderFeatureContext MakeFeatureContext();

        // ── Features ─────────────────────────────────────────
        std::vector<std::unique_ptr<RenderFeature>> m_features;

        // ── Debug-only pipeline (PosColour, uses debug_unlit shaders) ──
        GPUPipeline m_debugLinePipeline;

        // Single built-in mesh — all scene primitives use PosNormalColour
        Mesh m_primitiveMesh;

        int m_screenWidth;
        int m_screenHeight;

        bool m_isInitialised;
    };

} // namespace Wayfinder
