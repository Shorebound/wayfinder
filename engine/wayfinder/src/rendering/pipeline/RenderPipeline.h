#pragma once

#include "core/Types.h"
#include "rendering/graph/RenderFrame.h"
#include "rendering/graph/RenderPass.h"

#include <cstdint>
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

    /// Fixed ordering bands for engine-owned `RenderPass` injectors. Plugins register into these phases
    /// with `orderWithinPhase` for stable ordering within a band.
    enum class EngineRenderPhase : uint8_t
    {
        PreOpaque = 0,
        OpaqueMain = 1,
        PostOpaque = 2,
        Debug = 3,
        PreComposite = 4,
        /// Runs in the **late engine** segment — **after** game `RenderPass` injectors (e.g. present, swapchain composite).
        LateEngine = 5,
    };

    /// Validates, sorts, and builds the render graph for a frame.
    class WAYFINDER_API RenderPipeline
    {
    public:
        /// Registers built-in shader programs and stores context for BuildGraph.
        void Initialise(RenderContext& context);
        void Shutdown();

        /// Registers an engine pass (opaque, debug, optional plugins). Requires `Initialise` to have run.
        /// Passes are ordered by `(phase, orderWithinPhase, registration order)`.
        void RegisterEnginePass(EngineRenderPhase phase, int32_t orderWithinPhase, std::unique_ptr<RenderPass> pass);

        /// Validates views/layers, pre-computes view matrices and frustums,
        /// frustum-culls submissions, then sorts by sort key.
        /// Returns false if the frame is invalid and should be skipped.
        bool Prepare(RenderFrame& frame, uint32_t swapchainWidth, uint32_t swapchainHeight) const;

        /// Builds the render graph: **early engine** injectors → **game** injectors → **late engine** injectors.
        void BuildGraph(RenderGraph& graph, const RenderPipelineFrameParams& params, std::span<const std::unique_ptr<RenderPass>> gamePasses) const;

    private:
        struct EnginePassSlot
        {
            EngineRenderPhase Phase = EngineRenderPhase::PreOpaque;
            int32_t OrderWithinPhase = 0;
            uint32_t InsertSequence = 0;
            std::unique_ptr<RenderPass> Pass;
        };

        static void SortEnginePassList(std::vector<EnginePassSlot>& slots);

        void InvokePassList(RenderGraph& graph, const RenderPipelineFrameParams& params, const std::vector<EnginePassSlot>& slots) const;

        RenderContext* m_context = nullptr;
        std::vector<EnginePassSlot> m_earlyEnginePasses;
        std::vector<EnginePassSlot> m_lateEnginePasses;
        uint32_t m_nextEnginePassInsertSequence = 0;
        bool m_initialised = false;
    };
} // namespace Wayfinder
