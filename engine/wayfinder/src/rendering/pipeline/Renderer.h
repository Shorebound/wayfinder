#pragma once

#include "core/Result.h"
#include "rendering/RenderTypes.h"
#include "rendering/graph/RenderFeature.h"
#include "rendering/pipeline/RenderOrchestrator.h"

#include <memory>
#include <string>
#include <vector>

namespace Wayfinder
{
    class AssetService;
    class BlendableEffectRegistry;
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

        Result<void> Initialise(RenderDevice& device, const EngineConfig& config, BlendableEffectRegistry* registry = nullptr);
        void Shutdown();

        void Render(const RenderFrame& frame);
        void SetAssetService(const std::shared_ptr<AssetService>& assetService);

        /**
         * @brief Seals the blendable effect registry, preventing further registrations.
         *
         * Call after all game/editor blendable effect types have been registered.
         * Engine types are registered during Initialise; this finalises the registry.
         * `Render()` also seals automatically if the registry is not yet sealed, so this is only
         * required when something must observe a sealed registry before the first `Render()` call.
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
         * @brief Registers a render feature with default order within the phase.
         * @param phase Band used with `order` for stable ordering.
         * @param pass Ownership of the pass instance; must not be null.
         * @note Order is `0` (same as calling `AddPass(phase, 0, std::move(pass))`).
         */
        void AddPass(RenderPhase phase, std::unique_ptr<RenderFeature> pass);

        /**
         * @brief Removes the first pass whose dynamic type is `T` from the pipeline.
         * @tparam T Render pass type to match.
         * @return True if a pass was removed; false if none matched.
         */
        template<typename T>
        bool RemovePass()
        {
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
                const auto* rp = m_renderPipeline.get();
                return rp->GetPass<T>();
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
                return m_renderPipeline->GetPass<T>();
            }
            return nullptr;
        }

    private:
        std::shared_ptr<AssetService> m_assetService;
        RenderDevice* m_device = nullptr;
        std::unique_ptr<RenderServices> m_services;
        std::unique_ptr<RenderOrchestrator> m_renderPipeline;
        std::unique_ptr<RenderResourceCache> m_renderResources;

        int m_screenWidth;
        int m_screenHeight;

        bool m_isInitialised;
    };

} // namespace Wayfinder
