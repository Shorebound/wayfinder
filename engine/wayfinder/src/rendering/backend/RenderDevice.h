#pragma once

#include <array>
#include <memory>
#include <string_view>
#include <utility>

#include "VertexFormats.h"
#include "core/Result.h"
#include "rendering/RenderTypes.h"

namespace Wayfinder
{
    class Window;

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
        const uint8_t* Code = nullptr;
        size_t CodeSize = 0;
        const char* EntryPoint = "main";
        ShaderStage Stage = ShaderStage::Vertex;
        uint32_t Samplers = 0;
        uint32_t StorageTextures = 0;
        uint32_t StorageBuffers = 0;
        uint32_t UniformBuffers = 0;
    };

    // Shader resource counts -- describes the resource bindings a shader uses.
    // Supplied to RenderDevice::CreateShader. Resolved automatically by ShaderManager
    // from: Slang reflection (runtime), build-time shader_manifest.json, or SPIR-V analysis.
    struct ShaderResourceCounts
    {
        uint32_t UniformBuffers = 0;
        uint32_t Samplers = 0;
        uint32_t StorageTextures = 0;
        uint32_t StorageBuffers = 0;
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

    // ── Blend State ─────────────────────────────────────────

    /** @brief Source or destination factor applied during colour/alpha blending. */
    enum class BlendFactor : uint8_t
    {
        Zero,
        One,
        SrcAlpha,
        OneMinusSrcAlpha,
        DstAlpha,
        OneMinusDstAlpha,
        SrcColour,
        OneMinusSrcColour,
        DstColour,
        OneMinusDstColour,
        ConstantColour,
        OneMinusConstantColour,
    };

    /** @brief Arithmetic operation used to combine source and destination blend terms. */
    enum class BlendOp : uint8_t
    {
        Add,
        Subtract,
        ReverseSubtract,
        Min,
        Max,
    };

    /**
     * @brief Per-target colour blending configuration.
     *
     * When Enabled is false the target writes opaque fragments with no blending.
     * ColourWriteMask is a 4-bit RGBA mask (0xF = all channels).
     */
    struct BlendState
    {
        bool Enabled = false;
        BlendFactor SrcColourFactor = BlendFactor::SrcAlpha;
        BlendFactor DstColourFactor = BlendFactor::OneMinusSrcAlpha;
        BlendOp ColourOp = BlendOp::Add;
        BlendFactor SrcAlphaFactor = BlendFactor::One;
        BlendFactor DstAlphaFactor = BlendFactor::OneMinusSrcAlpha;
        BlendOp AlphaOp = BlendOp::Add;
        uint8_t ColourWriteMask = 0xF;

        constexpr bool operator==(const BlendState&) const = default;
    };

    /** @brief Factory functions returning common blend configurations. */
    namespace BlendPresets
    {
        /** @return Disabled blending (opaque fragments). */
        constexpr BlendState Opaque()
        {
            return {};
        }

        /** @return Standard alpha blending (src·α + dst·(1−α)). */
        constexpr BlendState AlphaBlend()
        {
            return {true, BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha, BlendOp::Add, BlendFactor::One, BlendFactor::OneMinusSrcAlpha, BlendOp::Add};
        }

        /** @return Additive blending (src·α + dst). */
        constexpr BlendState Additive()
        {
            return {true, BlendFactor::SrcAlpha, BlendFactor::One, BlendOp::Add, BlendFactor::SrcAlpha, BlendFactor::One, BlendOp::Add};
        }

        /** @return Pre-multiplied alpha blending (src + dst·(1−α)). */
        constexpr BlendState Premultiplied()
        {
            return {true, BlendFactor::One, BlendFactor::OneMinusSrcAlpha, BlendOp::Add, BlendFactor::One, BlendFactor::OneMinusSrcAlpha, BlendOp::Add};
        }

        /** @return Multiplicative blending (src·dst + 0). */
        constexpr BlendState Multiplicative()
        {
            return {true, BlendFactor::DstColour, BlendFactor::Zero, BlendOp::Add, BlendFactor::DstAlpha, BlendFactor::Zero, BlendOp::Add};
        }
    }

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
        BufferUsage Usage = BufferUsage::Vertex;
        uint32_t SizeInBytes = 0;
    };

    struct BufferUploadRegion
    {
        uint32_t SizeInBytes = 0;
        uint32_t DstOffsetInBytes = 0;
    };

    struct VertexBufferBindingDesc
    {
        uint32_t Slot = 0;
        uint32_t OffsetInBytes = 0;
    };

    // ── Pipeline Create Descriptor ───────────────────────────

    struct PipelineCreateDesc
    {
        GPUShaderHandle VertexShader{};
        GPUShaderHandle FragmentShader{};
        VertexLayout VertexLayout{};
        PrimitiveType PrimitiveType = PrimitiveType::TriangleList;
        CullMode CullMode = CullMode::Back;
        FillMode FillMode = FillMode::Fill;
        FrontFace FrontFace = FrontFace::CounterClockwise;
        bool DepthTestEnabled = false;
        bool DepthWriteEnabled = false;
        uint32_t ColourTargetCount = 1;
        std::array<TextureFormat, MAX_COLOUR_TARGETS> ColourTargetFormats{};
        std::array<BlendState, MAX_COLOUR_TARGETS> ColourTargetBlends{};
    };

    // ── Compute Pipeline Create Descriptor ───────────────────

    struct ComputePipelineCreateDesc
    {
        const uint8_t* Code = nullptr;
        size_t CodeSize = 0;
        const char* EntryPoint = "main";
        uint32_t Samplers = 0;
        uint32_t ReadOnlyStorageTextures = 0;
        uint32_t ReadOnlyStorageBuffers = 0;
        uint32_t ReadWriteStorageTextures = 0;
        uint32_t ReadWriteStorageBuffers = 0;
        uint32_t UniformBuffers = 0;
        uint32_t ThreadCountX = 1;
        uint32_t ThreadCountY = 1;
        uint32_t ThreadCountZ = 1;
    };

    class WAYFINDER_API RenderDevice
    {
    public:
        virtual ~RenderDevice() = default;

        /// ── Lifecycle ────────────────────────────────────────

        virtual Result<void> Initialise(Window& window) = 0;
        virtual void Shutdown() = 0;

        /// ── Frame Lifecycle ──────────────────────────────────
        /**
         * @brief Acquire the command buffer and swapchain texture for this frame.
         * @return False if the swapchain is unavailable, such as when the window is minimised.
         *
         * When this returns false, skip rendering work for the frame and call EndFrame().
         */
        virtual bool BeginFrame() = 0;

        /// Submit the command buffer and present the swapchain.
        virtual void EndFrame() = 0;

        // ── GPU Debug Annotation ─────────────────────────────

        virtual void PushDebugGroup(std::string_view name) = 0;
        virtual void PopDebugGroup() = 0;

        // ── Render Pass ──────────────────────────────────────

        /**
         * @brief Begin a render pass described by the given descriptor.
         * @return True if the pass was started successfully; false if it was skipped
         *         (missing swapchain, null targets, etc.).  Callers should skip
         *         Execute / EndRenderPass when this returns false.
         */
        virtual bool BeginRenderPass(const RenderPassDescriptor& descriptor) = 0;
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
        virtual void UploadToBuffer(GPUBufferHandle buffer, const void* data, BufferUploadRegion region) = 0;

        /**
         * @brief Submit all pending staged uploads in a single GPU copy pass.
         *
         * Backends that batch uploads internally should override this to flush.
         * Called once per frame after PrepareFrame and before render graph execution.
         * The default implementation is a no-op for backends without staging.
         */
        virtual void FlushUploads() {}

        // ── Draw Commands ────────────────────────────────────────

        virtual void BindVertexBuffer(GPUBufferHandle buffer, VertexBufferBindingDesc binding = {}) = 0;
        virtual void BindIndexBuffer(GPUBufferHandle buffer, IndexElementSize indexSize, uint32_t offsetInBytes = 0) = 0;
        virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0) = 0;
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

        /**
         * @brief Upload pixel data to a specific mip level of a GPU texture.
         *
         * The texture must have been created with Sampler usage.
         * Pixel data must be tightly packed (bytesPerRow == width * bytesPerPixel).
         *
         * @param texture  GPU texture handle to upload to.
         * @param pixelData  Pointer to the source pixel data for this mip level.
         * @param width  Width of this mip level in pixels.
         * @param height  Height of this mip level in pixels.
         * @param bytesPerRow  Bytes per row.
         * @param mipLevel  Target mip level (0 = base).
         */
        virtual void UploadToTexture(GPUTextureHandle texture, const void* pixelData, uint32_t width, uint32_t height, uint32_t bytesPerRow, uint32_t mipLevel = 0) = 0;

        /**
         * @brief Generate mipmaps for a texture.
         *
         * Downsamples from mip level 0 to all subsequent levels. The texture
         * must have been created with mipLevels > 1 and appropriate usage flags.
         * The device reads mip count and dimensions from its internal metadata.
         *
         * @param texture  GPU texture handle.
         */
        virtual void GenerateMipmaps(GPUTextureHandle texture) = 0;

        // ── Samplers ─────────────────────────────────────────────

        virtual GPUSamplerHandle CreateSampler(const SamplerCreateDesc& desc) = 0;
        virtual void DestroySampler(GPUSamplerHandle sampler) = 0;

        virtual void BindFragmentSampler(uint32_t slot, GPUTextureHandle texture, GPUSamplerHandle sampler) = 0;

        // ── Swapchain Info ───────────────────────────────────────

        [[nodiscard]] virtual Extent2D GetSwapchainDimensions() const = 0;
        // ── Device Info ──────────────────────────────────────

        virtual const RenderDeviceInfo& GetDeviceInfo() const = 0;

        // ── Factory ──────────────────────────────────────────

        static std::unique_ptr<RenderDevice> Create(RenderBackend backend);
    };

    class WAYFINDER_API GPUDebugScope
    {
    public:
        GPUDebugScope(RenderDevice& device, std::string_view name) : m_device(&device)
        {
            m_device->PushDebugGroup(name);
        }

        ~GPUDebugScope()
        {
            Release();
        }

        GPUDebugScope(const GPUDebugScope&) = delete;
        GPUDebugScope& operator=(const GPUDebugScope&) = delete;

        GPUDebugScope(GPUDebugScope&& other) noexcept : m_device(std::exchange(other.m_device, nullptr)) {}

        GPUDebugScope& operator=(GPUDebugScope&& other) noexcept
        {
            if (this != &other)
            {
                Release();
                m_device = std::exchange(other.m_device, nullptr);
            }
            return *this;
        }

    private:
        void Release()
        {
            if (m_device != nullptr)
            {
                m_device->PopDebugGroup();
                m_device = nullptr;
            }
        }

        RenderDevice* m_device = nullptr;
    };

} // namespace Wayfinder
