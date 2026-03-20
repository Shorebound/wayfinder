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

    // ── GPU Handle Aliases ────────────────────────────────────

    using GPUShaderHandle          = Handle<GPUShaderTag>;
    using GPUPipelineHandle        = Handle<GPUPipelineTag>;
    using GPUBufferHandle          = Handle<GPUBufferTag>;
    using GPUTextureHandle         = Handle<GPUTextureTag>;
    using GPUSamplerHandle         = Handle<GPUSamplerTag>;
    using GPUComputePipelineHandle = Handle<GPUComputePipelineTag>;

} // namespace Wayfinder
