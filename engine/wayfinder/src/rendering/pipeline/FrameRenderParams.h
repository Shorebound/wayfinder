#pragma once

#include "rendering/RenderTypes.h"
#include "rendering/graph/RenderFrame.h"
#include "rendering/graph/RenderFrameUtils.h"

#include <array>
#include <cstdint>
#include <memory>

namespace Wayfinder
{
    class Mesh;
    class RenderResourceCache;

    /** @brief Identifies a built-in primitive mesh owned by the engine. */
    enum class BuiltInMeshId : uint8_t
    {
        PrimitiveColour,   ///< PosNormalColour vertex layout.
        PrimitiveTextured, ///< PosNormalUVTangent vertex layout.
        COUNT
    };

    inline constexpr size_t BUILT_IN_MESH_COUNT = static_cast<size_t>(BuiltInMeshId::COUNT);

    /** @brief Fixed-size array mapping `BuiltInMeshId` → `Mesh*`. */
    using BuiltInMeshTable = std::array<Mesh*, BUILT_IN_MESH_COUNT>;

    /// Per-frame inputs passed to BuildGraph and to each `RenderFeature::AddPasses`, bundled into a struct to
    /// keep the signature clean.
    struct FrameRenderParams
    {
        const RenderFrame& Frame;
        uint32_t SwapchainWidth;
        uint32_t SwapchainHeight;

        /// Built-in primitive meshes indexed by `BuiltInMeshId`.
        const BuiltInMeshTable& BuiltInMeshes;

        /// Used when `RenderMeshRef::Origin` is `Asset` — resolves cached GPU meshes.
        RenderResourceCache* ResourceCache = nullptr;

        /// Primary prepared view (`Rendering::ResolvePreparedPrimaryView`); used by graph passes for defaults.
        Rendering::PreparedPrimaryView PrimaryView{};
    };

} // namespace Wayfinder
