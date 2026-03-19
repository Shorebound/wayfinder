#pragma once

#include <memory>

#include "RenderTypes.h"
#include "VertexFormats.h"

namespace Wayfinder
{
    class Window;

    // ── GPU Handle Types ─────────────────────────────────────

    // Opaque handles wrapping backend-specific objects.
    // Engines hold these; only the RenderDevice knows the concrete type.
    using GPUShaderHandle = void*;
    using GPUPipelineHandle = void*;
    using GPUBufferHandle = void*;
    using GPUComputePipelineHandle = void*;

    // ── Shader Stage ─────────────────────────────────────────

    enum class ShaderStage : uint8_t
    {
        Vertex,
        Fragment,
        Compute,
    };

    // ── Shader Create Descriptor ─────────────────────────────

    struct ShaderCreateDesc
    {
        const uint8_t* code = nullptr;
        size_t codeSize = 0;
        const char* entryPoint = "main";
        ShaderStage stage = ShaderStage::Vertex;
        uint32_t numSamplers = 0;
        uint32_t numStorageTextures = 0;
        uint32_t numStorageBuffers = 0;
        uint32_t numUniformBuffers = 0;
    };

    // Shader resource counts — describes the resource bindings a shader uses.
    // Carried by GPUPipelineDesc so ShaderManager can create shaders with correct metadata.
    struct ShaderResourceCounts
    {
        uint32_t numUniformBuffers = 0;
        uint32_t numSamplers = 0;
        uint32_t numStorageTextures = 0;
        uint32_t numStorageBuffers = 0;
    };

    // ── Pipeline Create Descriptor ───────────────────────────

    enum class PrimitiveType : uint8_t
    {
        TriangleList,
        TriangleStrip,
        LineList,
        LineStrip,
        PointList,
    };

    enum class CullMode : uint8_t
    {
        None,
        Front,
        Back,
    };

    enum class FillMode : uint8_t
    {
        Fill,
        Line,
    };

    enum class FrontFace : uint8_t
    {
        CounterClockwise,
        Clockwise,
    };

    // ── Buffer Enums / Descriptors ──────────────────────────

    enum class BufferUsage : uint8_t
    {
        Vertex,
        Index,
    };

    enum class IndexElementSize : uint8_t
    {
        Uint16,
        Uint32,
    };

    struct BufferCreateDesc
    {
        BufferUsage usage = BufferUsage::Vertex;
        uint32_t sizeInBytes = 0;
    };

    // ── Pipeline Create Descriptor ───────────────────────────

    struct PipelineCreateDesc
    {
        GPUShaderHandle vertexShader = nullptr;
        GPUShaderHandle fragmentShader = nullptr;
        VertexLayout vertexLayout{};
        PrimitiveType primitiveType = PrimitiveType::TriangleList;
        CullMode cullMode = CullMode::Back;
        FillMode fillMode = FillMode::Fill;
        FrontFace frontFace = FrontFace::CounterClockwise;
        bool depthTestEnabled = false;
        bool depthWriteEnabled = false;
        // Stage 6: Blend state, depth format, multiple color targets
    };

    // ── Compute Pipeline Create Descriptor ───────────────────

    struct ComputePipelineCreateDesc
    {
        const uint8_t* code = nullptr;
        size_t codeSize = 0;
        const char* entryPoint = "main";
        uint32_t numSamplers = 0;
        uint32_t numReadOnlyStorageTextures = 0;
        uint32_t numReadOnlyStorageBuffers = 0;
        uint32_t numReadWriteStorageTextures = 0;
        uint32_t numReadWriteStorageBuffers = 0;
        uint32_t numUniformBuffers = 0;
        uint32_t threadCountX = 1;
        uint32_t threadCountY = 1;
        uint32_t threadCountZ = 1;
    };

    class WAYFINDER_API RenderDevice
    {
    public:
        virtual ~RenderDevice() = default;

        // ── Lifecycle ────────────────────────────────────────

        virtual bool Initialize(Window& window) = 0;
        virtual void Shutdown() = 0;

        // ── Frame Lifecycle ──────────────────────────────────

        // Acquires a command buffer and the swapchain texture for this frame.
        // Returns false if the swapchain is unavailable (e.g. window minimized). 
        // When false, skip rendering and call EndFrame().
        virtual bool BeginFrame() = 0;

        // Submits the command buffer and presents the swapchain.
        virtual void EndFrame() = 0;

        // ── Render Pass ──────────────────────────────────────

        virtual void BeginRenderPass(const RenderPassDescriptor& descriptor) = 0;
        virtual void EndRenderPass() = 0;

        // ── Shader and Pipeline ──────────────────────────────

        virtual GPUShaderHandle CreateShader(const ShaderCreateDesc& desc) = 0;
        virtual void DestroyShader(GPUShaderHandle shader) = 0;

        virtual GPUPipelineHandle CreatePipeline(const PipelineCreateDesc& desc) = 0;
        virtual void DestroyPipeline(GPUPipelineHandle pipeline) = 0;

        virtual void BindPipeline(GPUPipelineHandle pipeline) = 0;
        // ── Buffers ──────────────────────────────────────────────

        virtual GPUBufferHandle CreateBuffer(const BufferCreateDesc& desc) = 0;
        virtual void DestroyBuffer(GPUBufferHandle buffer) = 0;
        virtual void UploadToBuffer(GPUBufferHandle buffer, const void* data, uint32_t sizeInBytes, uint32_t dstOffsetInBytes = 0) = 0;

        // ── Draw Commands ────────────────────────────────────────

        virtual void BindVertexBuffer(GPUBufferHandle buffer, uint32_t slot = 0, uint32_t offsetInBytes = 0) = 0;
        virtual void BindIndexBuffer(GPUBufferHandle buffer, IndexElementSize indexSize, uint32_t offsetInBytes = 0) = 0;
        virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1,
                                 uint32_t firstIndex = 0, int32_t vertexOffset = 0) = 0;
        virtual void DrawPrimitives(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0) = 0;
        virtual void PushVertexUniform(uint32_t slot, const void* data, uint32_t sizeInBytes) = 0;
        virtual void PushFragmentUniform(uint32_t slot, const void* data, uint32_t sizeInBytes) = 0;

        // ── Compute ──────────────────────────────────────────────

        virtual GPUComputePipelineHandle CreateComputePipeline(const ComputePipelineCreateDesc& desc) = 0;
        virtual void DestroyComputePipeline(GPUComputePipelineHandle pipeline) = 0;
        virtual void BeginComputePass() = 0;
        virtual void EndComputePass() = 0;
        virtual void BindComputePipeline(GPUComputePipelineHandle pipeline) = 0;
        virtual void DispatchCompute(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;
        // ── Textures ─────────────────────────────────────────────

        virtual GPUTextureHandle CreateTexture(const TextureCreateDesc& desc) = 0;
        virtual void DestroyTexture(GPUTextureHandle texture) = 0;

        // ── Samplers ─────────────────────────────────────────────

        virtual GPUSamplerHandle CreateSampler(const SamplerCreateDesc& desc) = 0;
        virtual void DestroySampler(GPUSamplerHandle sampler) = 0;

        virtual void BindFragmentSampler(uint32_t slot, GPUTextureHandle texture, GPUSamplerHandle sampler) = 0;

        // ── Swapchain Info ───────────────────────────────────────

        virtual void GetSwapchainDimensions(uint32_t& width, uint32_t& height) const = 0;
        // ── Device Info ──────────────────────────────────────

        virtual const RenderDeviceInfo& GetDeviceInfo() const = 0;

        // ── Factory ──────────────────────────────────────────

        static std::unique_ptr<RenderDevice> Create(RenderBackend backend);
    };

} // namespace Wayfinder
