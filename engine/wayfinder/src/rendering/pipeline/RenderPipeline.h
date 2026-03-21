#pragma once

#include "core/Types.h"
#include "rendering/graph/RenderFrame.h"
#include "rendering/RenderTypes.h"

#include <memory>
#include <span>
#include <type_traits>

namespace Wayfinder
{
    class GPUPipeline;
    class Mesh;
    class RenderContext;
    class RenderDevice;
    class RenderFeature;
    class RenderGraph;
    class RenderResourceCache;

    // Per-frame scene globals pushed to fragment UBO slot 1 for shaders that need it.
    struct alignas(16) SceneGlobalsUBO
    {
        Float3 LightDirection{0.0f, -0.7f, -0.5f};
        float LightIntensity = 1.0f;
        Float3 LightColour{1.0f, 1.0f, 1.0f};
        float Ambient = 0.15f;
    };
    static_assert(std::is_standard_layout_v<SceneGlobalsUBO>, "SceneGlobalsUBO must be standard layout for GPU upload");
    static_assert(std::is_trivially_copyable_v<SceneGlobalsUBO>, "SceneGlobalsUBO must be trivially copyable for GPU upload");
    static_assert(sizeof(SceneGlobalsUBO) == 32, "SceneGlobalsUBO must be 32 bytes (2 x vec4) for std140 layout");

    /// Per-frame inputs passed to BuildGraph, bundled into a struct to
    /// keep the signature clean.
    struct RenderPipelineFrameParams
    {
        const RenderFrame& Frame;
        uint32_t SwapchainWidth;
        uint32_t SwapchainHeight;
        Mesh& PrimitiveMesh;
        Mesh& TexturedPrimitiveMesh;
        GPUPipeline& DebugLinePipeline;
        std::span<const std::unique_ptr<RenderFeature>> Features;
    };

    /// Validates, sorts, and builds the render graph for a frame.
    class WAYFINDER_API RenderPipeline
    {
    public:
        /// Registers built-in shader programs and stores context for BuildGraph.
        void Initialise(RenderContext& context);
        void Shutdown();

        /// Validates views/passes, sorts scene submissions by sort key.
        /// Returns false if the frame is invalid and should be skipped.
        bool Prepare(RenderFrame& frame) const;

        /// Builds the full render graph for the frame (MainScene, Debug,
        /// Feature passes, Composition).
        void BuildGraph(RenderGraph& graph, const RenderPipelineFrameParams& params) const;

    private:
        RenderContext* m_context = nullptr;

        /// Extracts the primary directional light from the frame.
        SceneGlobalsUBO BuildSceneGlobals(const RenderFrame& frame) const;
    };
}