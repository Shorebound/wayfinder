#pragma once

#include "core/Types.h"
#include "rendering/graph/RenderPass.h"

#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace Wayfinder
{
    class AssetService;
    class Mesh;
    class MeshManager;
    class RenderContext;
    class RenderDevice;
    class RenderGraph;
    class RenderResourceCache;

    /// Validates, sorts, and builds the render graph for a frame.
    class WAYFINDER_API RenderPipeline
    {
    public:
        /// Registers built-in shader programs and stores context for BuildGraph.
        void Initialise(RenderContext& context);
        void Shutdown();

        /// Validates views/passes, pre-computes view matrices and frustums,
        /// frustum-culls submissions, then sorts by sort key.
        /// Returns false if the frame is invalid and should be skipped.
        bool Prepare(RenderFrame& frame, uint32_t swapchainWidth, uint32_t swapchainHeight) const;

        /// Builds the full render graph for the frame (engine passes, game passes, Composition).
        void BuildGraph(RenderGraph& graph, const RenderPipelineFrameParams& params, std::span<const std::unique_ptr<RenderPass>> gamePasses) const;

    private:
        void AddEnginePass(std::unique_ptr<RenderPass> pass);

        RenderContext* m_context = nullptr;
        std::vector<std::unique_ptr<RenderPass>> m_enginePasses;
    };
} // namespace Wayfinder
