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

    // ── Shader Stage ─────────────────────────────────────────

    enum class ShaderStage : uint8_t
    {
        Vertex,
        Fragment,
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

        // ── Device Info ──────────────────────────────────────

        virtual const RenderDeviceInfo& GetDeviceInfo() const = 0;

        // ── Factory ──────────────────────────────────────────

        static std::unique_ptr<RenderDevice> Create(RenderBackend backend);
    };

} // namespace Wayfinder
