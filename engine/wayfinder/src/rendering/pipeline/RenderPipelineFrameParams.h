#pragma once

#include "rendering/RenderTypes.h"
#include "rendering/graph/RenderFrame.h"
#include "rendering/graph/RenderFrameUtils.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace Wayfinder
{
    class Mesh;
    class RenderResourceCache;

    /// Per-frame inputs passed to BuildGraph and to each `RenderPass::AddPasses`, bundled into a struct to
    /// keep the signature clean.
    struct RenderPipelineFrameParams
    {
        const RenderFrame& Frame;
        uint32_t SwapchainWidth;
        uint32_t SwapchainHeight;

        /// Mesh lookup by vertex layout stride. The draw loop selects
        /// the correct mesh for each shader program's declared vertex format.
        const std::unordered_map<uint32_t, Mesh*>& MeshesByStride;

        /// Used when `RenderMeshRef::Origin` is `Asset` — resolves cached GPU meshes.
        RenderResourceCache* ResourceCache = nullptr;

        /// Primary prepared view (`ResolvePreparedPrimaryView`); used by graph passes for defaults.
        PreparedPrimaryView PrimaryView{};
    };

} // namespace Wayfinder
