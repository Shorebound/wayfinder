#pragma once

#include "rendering/RenderTypes.h"

#include <string>

namespace Wayfinder
{
    class RenderGraph;
    class RenderDevice;
    class PipelineCache;
    class ShaderManager;
    class ShaderProgramRegistry;
    struct RenderFrame;

    // Services made available to features during OnAttach / OnDetach.
    struct RenderFeatureContext
    {
        RenderDevice& Device;
        ShaderProgramRegistry& ProgramRegistry;
        ShaderManager& ShaderManager;
        PipelineCache& PipelineCache;
        GPUSamplerHandle NearestSampler{};
    };

    // Base class for registerable rendering extensions.
    // Not an interface (holds state via m_enabled), so the I-prefix is intentionally omitted.
    // A RenderFeature injects one or more passes into the per-frame render graph.
    // Game developers create custom features to add post-processing, overlays,
    // debug visualizations, or any rendering work without modifying engine code.
    //
    // Usage:
    //   renderer.AddFeature(std::make_unique<MyFeature>());
    //
    // Features are called after engine core passes (MainScene, Debug) are added.
    // Execution order is determined by resource dependencies in the graph,
    // not by registration order.

    class RenderFeature
    {
    public:
        virtual ~RenderFeature() = default;

        // Returns a unique name for this feature (used for removal and logging).
        virtual const std::string& GetName() const = 0;

        // Called each frame to inject passes into the render graph.
        // The feature should declare resource dependencies so the graph
        // can determine correct execution order.
        virtual void AddPasses(RenderGraph& graph, const RenderFrame& frame) = 0;

        // Called when the feature is first registered. Use the context to register
        // shader programs, create pipelines, or acquire GPU resources.
        virtual void OnAttach(const RenderFeatureContext& /*context*/) {}

        // Called when the feature is removed. Use the context for cleanup.
        virtual void OnDetach(const RenderFeatureContext& /*context*/) {}

        bool IsEnabled() const
        {
            return m_enabled;
        }
        void SetEnabled(bool enabled)
        {
            m_enabled = enabled;
        }

    private:
        bool m_enabled = true;
    };

} // namespace Wayfinder
