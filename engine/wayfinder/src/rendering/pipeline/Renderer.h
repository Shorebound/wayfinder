#pragma once

#include "core/Result.h"
#include "rendering/RenderTypes.h"
#include "rendering/graph/RenderPass.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/pipeline/RenderPipeline.h"

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
    class RenderResourceCache;

    class WAYFINDER_API Renderer
    {
    public:
        Renderer();
        ~Renderer() noexcept;

        Result<void> Initialise(RenderDevice& device, const EngineConfig& config);
        void Shutdown();

        void Render(const RenderFrame& frame);
        void SetAssetService(const std::shared_ptr<AssetService>& assetService);

        /**
         * @brief Registers a game or editor-owned render pass; it receives `OnAttach` immediately if the renderer is initialised.
         * @param pass Ownership of the pass instance; must not be null.
         */
        void AddPass(std::unique_ptr<RenderPass> pass);

        /**
         * @brief Registers an engine pass in the fixed phase ordering (opaque, debug, etc.).
         * @param phase Band used with `orderWithinPhase` for stable ordering within the engine pipeline.
         * @param orderWithinPhase Lower values run earlier within the same phase.
         * @param pass Ownership of the pass instance; must not be null.
         */
        void RegisterEnginePass(EngineRenderPhase phase, int32_t orderWithinPhase, std::unique_ptr<RenderPass> pass);

        template<typename T>
        bool RemovePass()
        {
            auto it = std::find_if(m_passes.begin(), m_passes.end(), [](const std::unique_ptr<RenderPass>& p)
            {
                return dynamic_cast<T*>(p.get()) != nullptr;
            });
            if (it != m_passes.end())
            {
                if (m_isInitialised && m_context)
                {
                    auto ctx = MakePassContext();
                    (*it)->OnDetach(ctx);
                }
                m_passes.erase(it);
                return true;
            }
            return false;
        }

        template<typename T>
        const T* GetPass() const
        {
            for (const auto& p : m_passes)
            {
                if (auto* ptr = dynamic_cast<const T*>(p.get()))
                {
                    return ptr;
                }
            }
            return nullptr;
        }

        template<typename T>
        T* GetPass()
        {
            for (auto& p : m_passes)
            {
                if (auto* ptr = dynamic_cast<T*>(p.get()))
                {
                    return ptr;
                }
            }
            return nullptr;
        }

    private:
        std::shared_ptr<AssetService> m_assetService;
        RenderDevice* m_device = nullptr;
        std::unique_ptr<RenderContext> m_context;
        std::unique_ptr<RenderPipeline> m_renderPipeline;
        std::unique_ptr<RenderResourceCache> m_renderResources;

        RenderPassContext MakePassContext();

        std::vector<std::unique_ptr<RenderPass>> m_passes;

        Mesh m_primitiveMesh;
        Mesh m_texturedPrimitiveMesh;

        int m_screenWidth;
        int m_screenHeight;

        bool m_isInitialised;
    };

} // namespace Wayfinder
