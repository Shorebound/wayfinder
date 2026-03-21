#pragma once

#include "core/Handle.h"

namespace Wayfinder
{
    // ── GPU Resource Tag Types ────────────────────────────────
    // Each tag creates a distinct Handle<T> type for compile-time safety.

    struct GPUShaderTag {};
    struct GPUPipelineTag {};
    struct GPUBufferTag {};
    struct GPUTextureTag {};
    struct GPUSamplerTag {};
    struct GPUComputePipelineTag {};

    // ── Render Resource Tag Types ─────────────────────────────

    /** @brief Tag type for GPU-resident mesh resources. */
    struct RenderMeshTag {};

    /** @brief Tag type for GPU-resident material resources. */
    struct RenderMaterialTag {};

    // ── GPU Handle Aliases ────────────────────────────────────

    using GPUShaderHandle          = Handle<GPUShaderTag>;
    using GPUPipelineHandle        = Handle<GPUPipelineTag>;
    using GPUBufferHandle          = Handle<GPUBufferTag>;
    using GPUTextureHandle         = Handle<GPUTextureTag>;
    using GPUSamplerHandle         = Handle<GPUSamplerTag>;
    using GPUComputePipelineHandle = Handle<GPUComputePipelineTag>;

    // ── Render Resource Handle Aliases ────────────────────────

    /**
     * @brief Type-safe generational handle for GPU-resident mesh resources.
     *
     * Distinct from `GPUBufferHandle`, `GPUTextureHandle`, and all other
     * `Handle<T>` specialisations — mixing them is a compile-time error.
     */
    using RenderMeshHandle = Handle<RenderMeshTag>;

    /**
     * @brief Type-safe generational handle for GPU-resident material resources.
     *
     * Distinct from `RenderMeshHandle`, `GPUBufferHandle`, `GPUTextureHandle`, and all
     * other `Handle<T>` specialisations — mixing them is a compile-time error.
     */
    using RenderMaterialHandle = Handle<RenderMaterialTag>;

} // namespace Wayfinder
