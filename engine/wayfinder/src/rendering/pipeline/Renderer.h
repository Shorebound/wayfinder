#pragma once

#include "core/Result.h"
#include "rendering/RenderTypes.h"
#include "rendering/graph/RenderFeature.h"
#include "rendering/pipeline/FrameComposer.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace Wayfinder
{
    class AssetService;
    class RenderServices;
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
         * @brief Seals the blendable effect registry, preventing further registrations.
         *
         * Call after all game/editor blendable effect types have been registered.
         * Engine types are registered during Initialise; this finalises the registry.
         */
        void SealBlendableEffects();

        /**
         * @brief Registers a render pass in the unified phase-ordered pipeline.
         * @param phase Band used with `order` for stable ordering.
         * @param order Lower values run earlier within the same phase.
         * @param pass Ownership of the pass instance; must not be null.
         */
        void AddPass(RenderPhase phase, int32_t order, std::unique_ptr<RenderFeature> pass);

        /**
         * @brief Registers a render feature with `order` 0 within the phase.
         */
        void AddPass(RenderPhase phase, std::unique_ptr<RenderFeature> pass);

        /**
         * @brief Removes the first pass whose dynamic type is `T` from the pipeline.
         * @tparam T Render pass type to match.
         * @return True if a pass was removed; false if none matched.
         * @note When the renderer is initialised, calls `OnDetach` on the removed pass before erasing it.
         */
        template<typename T>
        bool RemovePass()
        {
            auto pendingIt = std::find_if(m_pendingPasses.begin(), m_pendingPasses.end(), [](const PendingPassRegistration& p)
            {
                return p.Pass && dynamic_cast<T*>(p.Pass.get()) != nullptr;
            });
            if (pendingIt != m_pendingPasses.end())
            {
                if (m_isInitialised && m_context)
                {
                    auto ctx = MakeFeatureContext();
                    pendingIt->Pass->OnDetach(ctx);
                }
                m_pendingPasses.erase(pendingIt);
                return true;
            }

            if (m_renderPipeline)
            {
                return m_renderPipeline->RemovePass<T>();
            }
            return false;
        }

        /**
         * @brief Returns the first pass whose dynamic type is `T`, if any.
         * @tparam T Render pass type to match.
         * @return Pointer to the pass, or nullptr when no matching pass is registered.
         */
        template<typename T>
        const T* GetPass() const
        {
            if (m_renderPipeline)
            {
                if (const T* p = m_renderPipeline->GetPass<T>())
                {
                    return p;
                }
            }
            for (const auto& pending : m_pendingPasses)
            {
                if (pending.Pass)
                {
                    if (auto* ptr = dynamic_cast<const T*>(pending.Pass.get()))
                    {
                        return ptr;
                    }
                }
            }
            return nullptr;
        }

        /**
         * @brief Returns the first pass whose dynamic type is `T`, if any (non-const view).
         * @tparam T Render pass type to match.
         * @return Pointer to the pass, or nullptr when no matching pass is registered.
         */
        template<typename T>
        T* GetPass()
        {
            if (m_renderPipeline)
            {
                if (T* p = m_renderPipeline->GetPass<T>())
                {
                    return p;
                }
            }
            for (auto& pending : m_pendingPasses)
            {
                if (pending.Pass)
                {
                    if (auto* ptr = dynamic_cast<T*>(pending.Pass.get()))
                    {
                        return ptr;
                    }
                }
            }
            return nullptr;
        }

    private:
        struct PendingPassRegistration
        {
            RenderPhase Phase = RenderPhase::Opaque;
            int32_t Order = 0;
            std::unique_ptr<RenderFeature> Pass;
        };

        std::shared_ptr<AssetService> m_assetService;
        RenderDevice* m_device = nullptr;
        std::unique_ptr<RenderServices> m_context;
        std::unique_ptr<FrameComposer> m_renderPipeline;
        std::unique_ptr<RenderResourceCache> m_renderResources;

        RenderFeatureContext MakeFeatureContext();

        std::vector<PendingPassRegistration> m_pendingPasses;

        int m_screenWidth;
        int m_screenHeight;

        bool m_isInitialised;
    };

} // namespace Wayfinder
