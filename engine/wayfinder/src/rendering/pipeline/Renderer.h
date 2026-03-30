#pragma once

#include "core/Result.h"
#include "rendering/RenderTypes.h"
#include "rendering/graph/RenderFeature.h"
#include "rendering/pipeline/RenderOrchestrator.h"

#include <concepts>
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

        /**
         * @brief Initialises the renderer, creating internal services, resource caches, and the render pipeline.
         * @param device The render device used for GPU resource creation and command submission.
         * @param config Engine configuration (screen dimensions, etc.).
         * @param registry Optional blendable effect registry for external effect type registration.
         *                 Caller retains ownership. nullptr is valid and means no external blendable
         *                 registration will be performed. Must outlive the Renderer if provided.
         * @return Success, or an error describing the initialisation failure.
         */
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
         * @brief Registers a render feature in the unified phase-ordered pipeline.
         * @param phase Band used with `order` for stable ordering.
         * @param order Lower values run earlier within the same phase.
         * @param feature Ownership of the feature instance; must not be null.
         */
        void AddFeature(RenderPhase phase, int32_t order, std::unique_ptr<RenderFeature> feature);

        /**
         * @brief Registers a render feature with default order within the phase.
         * @param phase Band used with `order` for stable ordering.
         * @param feature Ownership of the feature instance; must not be null.
         * @note Order is `0` (same as calling `AddFeature(phase, 0, std::move(feature))`).
         */
        void AddFeature(RenderPhase phase, std::unique_ptr<RenderFeature> feature);

        /**
         * @brief Removes the first feature whose dynamic type is `T` from the pipeline.
         * @tparam TFeature Render feature type to match.
         * @return True if a feature was removed; false if none matched.
         */
        template<typename TFeature>
            requires std::derived_from<TFeature, RenderFeature>
        bool RemoveFeature()
        {
            if (m_renderPipeline)
            {
                return m_renderPipeline->RemoveFeature<TFeature>();
            }
            return false;
        }

        /**
         * @brief Returns the first feature whose dynamic type is `T`, if any.
         * @tparam TFeature Render feature type to match.
         * @return Pointer to the feature, or nullptr when no matching feature is registered.
         */
        template<typename TFeature>
            requires std::derived_from<TFeature, RenderFeature>
        const TFeature* GetFeature() const
        {
            if (m_renderPipeline)
            {
                const auto* rp = m_renderPipeline.get();
                return rp->GetFeature<TFeature>();
            }
            return nullptr;
        }

        /**
         * @brief Returns the first feature whose dynamic type is `T`, if any (non-const view).
         * @tparam TFeature Render feature type to match.
         * @return Pointer to the feature, or nullptr when no matching feature is registered.
         */
        template<typename TFeature>
            requires std::derived_from<TFeature, RenderFeature>
        TFeature* GetFeature()
        {
            if (m_renderPipeline)
            {
                return m_renderPipeline->GetFeature<TFeature>();
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
