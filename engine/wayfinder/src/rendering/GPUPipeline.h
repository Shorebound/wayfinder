#pragma once

#include "RenderDevice.h"
#include "VertexFormats.h"

#include <string>

namespace Wayfinder
{
    class ShaderManager;

    // Describes how to create a GPUPipeline: shader names + vertex layout + rasterizer state.
    struct GPUPipelineDesc
    {
        std::string vertexShaderName;
        std::string fragmentShaderName;
        ShaderResourceCounts vertexResources{.numUniformBuffers = 1};
        ShaderResourceCounts fragmentResources{};
        VertexLayout vertexLayout{};
        PrimitiveType primitiveType = PrimitiveType::TriangleList;
        CullMode cullMode = CullMode::Back;
        FillMode fillMode = FillMode::Fill;
        FrontFace frontFace = FrontFace::CounterClockwise;
        bool depthTestEnabled = false;
        bool depthWriteEnabled = false;
    };

    class PipelineCache;

    // Owns a GPU pipeline handle created from shader names + vertex layout + rasterizer config.
    // Resolves shader bytecode through ShaderManager, then calls RenderDevice::CreatePipeline.
    // If a PipelineCache is provided, the pipeline handle is retrieved from the cache
    // and Destroy() becomes a no-op (the cache owns the handle lifetime).
    class WAYFINDER_API GPUPipeline
    {
    public:
        GPUPipeline() = default;
        ~GPUPipeline() = default;

        GPUPipeline(const GPUPipeline&) = delete;
        GPUPipeline& operator=(const GPUPipeline&) = delete;

        bool Create(RenderDevice& device, ShaderManager& shaders, const GPUPipelineDesc& desc, PipelineCache* cache = nullptr);
        void Destroy();
        void Bind();

        bool IsValid() const { return m_pipeline != nullptr; }

    private:
        RenderDevice* m_device = nullptr;
        GPUPipelineHandle m_pipeline = nullptr;
        bool m_isFromCache = false;
    };

} // namespace Wayfinder
