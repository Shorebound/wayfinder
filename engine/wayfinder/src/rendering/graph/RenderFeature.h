#pragma once

#include "rendering/graph/RenderCapabilities.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/pipeline/FrameRenderParams.h"

#include <string_view>
#include <vector>

namespace Wayfinder
{
    class BlendableEffectRegistry;
    class RenderServices;
    class RenderGraph;

    /** @brief Services made available to features during OnAttach / OnDetach. */
    struct RenderFeatureContext
    {
        /** @brief The shared render services instance. */
        RenderServices& Context;
    };

    /**
     * @brief Base class for registerable rendering extensions (engine and game).
     *
     * A `RenderFeature` injects one or more passes into the per-frame render graph.
     *
     * Usage:
     *   renderer.AddPass(RenderPhase::PostProcess, 0, std::make_unique<MyFeature>());
     *
     * Registration order follows `RenderPhase` and `order` on the pipeline; GPU execution order is
     * still determined by resource dependencies in the graph where they apply.
     */
    class RenderFeature
    {
    public:
        virtual ~RenderFeature() = default;

        /** @brief Returns a unique name for this feature (used for removal and logging). */
        virtual std::string_view GetName() const = 0;

        /**
         * @brief Bitmask of `RenderCapabilities` — scheduling/profiling/authoring.
         *
         * Graph nodes may also call `RenderGraphBuilder::DeclarePassCapabilities` for dev-time checks
         * in `RenderGraph::Compile`.
         */
        virtual RenderCapabilityMask GetCapabilities() const
        {
            return RenderCapabilities::RASTER;
        }

        /** @brief Called each frame to inject passes into the render graph. */
        virtual void AddPasses(RenderGraph& graph, const FrameRenderParams& params) = 0;

        /**
         * @brief Called when the feature is first registered.
         *
         * Use the context to register shader programs, create pipelines, or acquire GPU resources.
         * Blendable effect types must be registered here; the registry is sealed on first `Renderer::Render()`.
         */
        virtual void OnAttach(const RenderFeatureContext& /*context*/) {}

        /**
         * @brief Returns shader program descriptors this feature requires.
         *
         * Called by the orchestrator at init and after each shader reload. Pure data - no side effects.
         * The orchestrator registers the returned descriptors with the ShaderProgramRegistry.
         */
        virtual std::vector<ShaderProgramDesc> GetShaderPrograms() const
        {
            return {};
        }

        /**
         * @brief Called once before the BlendableEffectRegistry is sealed.
         *
         * Register blendable effect types here. This is called before OnAttach, and only once
         * during the feature's lifetime (effect types cannot be added after the first frame).
         */
        virtual void OnRegisterEffects(BlendableEffectRegistry& /*registry*/) {}

        /** @brief Called when the feature is removed. Use the context for cleanup. */
        virtual void OnDetach(const RenderFeatureContext& /*context*/) {}

        /** @return True if this feature is enabled and will inject passes. */
        bool IsEnabled() const
        {
            return m_enabled;
        }

        /** @brief Enables or disables this feature. */
        virtual void SetEnabled(bool enabled)
        {
            m_enabled = enabled;
        }

    private:
        bool m_enabled = true;
    };

} // namespace Wayfinder
