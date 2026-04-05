#pragma once

#include "app/AppSubsystem.h"
#include "core/ResourcePool.h"
#include "core/Result.h"
#include "rendering/RenderTypes.h"
#include "rendering/backend/RenderDevice.h"
#include "wayfinder_exports.h"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

struct SDL_Window;
struct SDL_GPUDevice;
struct SDL_GPUCommandBuffer;
struct SDL_GPURenderPass;
struct SDL_GPUTexture;
struct SDL_GPUComputePass;
struct SDL_GPUShader;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUBuffer;
struct SDL_GPUTransferBuffer;
struct SDL_GPUSampler;
struct SDL_GPUComputePipeline;

namespace Wayfinder
{
    class EngineContext;
    class IMipGenerator;

    /** @brief Metadata stored alongside each SDL texture in the resource pool. */
    struct SDLTextureEntry
    {
        SDL_GPUTexture* Texture = nullptr;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t MipLevels = 1;
    };

    /**
     * @brief Direct SDL_GPU device management as an AppSubsystem.
     *
     * Owns the SDL_GPUDevice and all GPU resource pools. Replaces the three-tier
     * RenderDevice (abstract) + SDLGPUDevice (impl) + RenderDeviceSubsystem (wrapper)
     * with a single concrete type. Requires the Rendering capability.
     *
     * Registered by SDLRenderDevicePlugin with DependsOn SDLWindowSubsystem.
     */
    class WAYFINDER_API SDLRenderDeviceSubsystem final : public AppSubsystem
    {
    public:
        SDLRenderDeviceSubsystem();
        ~SDLRenderDeviceSubsystem() noexcept override;

        SDLRenderDeviceSubsystem(const SDLRenderDeviceSubsystem&) = delete;
        auto operator=(const SDLRenderDeviceSubsystem&) -> SDLRenderDeviceSubsystem& = delete;
        SDLRenderDeviceSubsystem(SDLRenderDeviceSubsystem&&) = delete;
        auto operator=(SDLRenderDeviceSubsystem&&) -> SDLRenderDeviceSubsystem& = delete;

        [[nodiscard]] auto Initialise(EngineContext& context) -> Result<void> override;
        void Shutdown() override;

        // ── Frame Lifecycle ──────────────────────────────────

        /**
         * @brief Acquire the command buffer and swapchain texture for this frame.
         * @return False if the swapchain is unavailable (e.g. window minimised).
         */
        [[nodiscard]] auto BeginFrame() -> bool;

        /// Submit the command buffer and present the swapchain.
        void EndFrame();

        // ── GPU Debug Annotation ─────────────────────────────

        void PushDebugGroup(std::string_view name);
        void PopDebugGroup();

        // ── Render Pass ──────────────────────────────────────

        /**
         * @brief Begin a render pass described by the given descriptor.
         * @return True if the pass was started successfully.
         */
        [[nodiscard]] auto BeginRenderPass(const RenderPassDescriptor& descriptor) -> bool;
        void EndRenderPass();

        // ── Shader and Pipeline ──────────────────────────────

        [[nodiscard]] auto CreateShader(const ShaderCreateDesc& desc) -> GPUShaderHandle;
        void DestroyShader(GPUShaderHandle shader);

        [[nodiscard]] auto CreatePipeline(const PipelineCreateDesc& desc) -> GPUPipelineHandle;
        void DestroyPipeline(GPUPipelineHandle pipeline);

        void BindPipeline(GPUPipelineHandle pipeline);

        // ── Buffers ──────────────────────────────────────────

        [[nodiscard]] auto CreateBuffer(const BufferCreateDesc& desc) -> GPUBufferHandle;
        void DestroyBuffer(GPUBufferHandle buffer);
        void UploadToBuffer(GPUBufferHandle buffer, const void* data, BufferUploadRegion region);
        void FlushUploads();

        // ── Draw Commands ────────────────────────────────────

        void BindVertexBuffer(GPUBufferHandle buffer, VertexBufferBindingDesc binding = {});
        void BindIndexBuffer(GPUBufferHandle buffer, IndexElementSize indexSize, uint32_t offsetInBytes = 0);
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0);
        void DrawPrimitives(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0);
        void PushVertexUniform(uint32_t slot, const void* data, uint32_t sizeInBytes);
        void PushFragmentUniform(uint32_t slot, const void* data, uint32_t sizeInBytes);

        // ── Compute ──────────────────────────────────────────

        [[nodiscard]] auto CreateComputePipeline(const ComputePipelineCreateDesc& desc) -> GPUComputePipelineHandle;
        void DestroyComputePipeline(GPUComputePipelineHandle pipeline);
        void BeginComputePass();
        void EndComputePass();
        void BindComputePipeline(GPUComputePipelineHandle pipeline);
        void DispatchCompute(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

        // ── Textures ─────────────────────────────────────────

        [[nodiscard]] auto CreateTexture(const TextureCreateDesc& desc) -> GPUTextureHandle;
        void DestroyTexture(GPUTextureHandle texture);
        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        void UploadToTexture(GPUTextureHandle texture, const void* pixelData, uint32_t width, uint32_t height, uint32_t bytesPerRow, uint32_t mipLevel = 0);
        void GenerateMipmaps(GPUTextureHandle texture);

        // ── Samplers ─────────────────────────────────────────

        [[nodiscard]] auto CreateSampler(const SamplerCreateDesc& desc) -> GPUSamplerHandle;
        void DestroySampler(GPUSamplerHandle sampler);

        void BindFragmentSampler(uint32_t slot, GPUTextureHandle texture, GPUSamplerHandle sampler);

        // ── Queries ──────────────────────────────────────────

        [[nodiscard]] auto GetSwapchainDimensions() const -> Extent2D;

        [[nodiscard]] auto GetDeviceInfo() const -> const RenderDeviceInfo&
        {
            return m_info;
        }

        [[nodiscard]] auto GetGPUDevice() const -> SDL_GPUDevice*
        {
            return m_gpuDevice;
        }

    private:
        SDL_GPUDevice* m_gpuDevice = nullptr;
        SDL_Window* m_window = nullptr;

        // Per-frame state
        SDL_GPUCommandBuffer* m_commandBuffer = nullptr;
        SDL_GPURenderPass* m_renderPass = nullptr;
        SDL_GPUTexture* m_swapchainTexture = nullptr;
        uint32_t m_swapchainWidth = 0;
        uint32_t m_swapchainHeight = 0;

        RenderDeviceInfo m_info;

        // Compute pass state
        SDL_GPUComputePass* m_computePass = nullptr;

        // Depth buffer -- owned by device, matched to swapchain dimensions
        SDL_GPUTexture* m_depthTexture = nullptr;
        uint32_t m_depthWidth = 0;
        uint32_t m_depthHeight = 0;
        void EnsureDepthTexture(uint32_t width, uint32_t height);

        // Shader format accepted by the chosen backend (queried at init)
        uint32_t m_shaderFormats = 0;

        // ── Resource Pools ───────────────────────────────────
        ResourcePool<GPUShaderTag, SDL_GPUShader*> m_shaderPool;
        ResourcePool<GPUPipelineTag, SDL_GPUGraphicsPipeline*> m_pipelinePool;
        ResourcePool<GPUBufferTag, SDL_GPUBuffer*> m_bufferPool;
        ResourcePool<GPUTextureTag, SDLTextureEntry> m_texturePool;
        ResourcePool<GPUSamplerTag, SDL_GPUSampler*> m_samplerPool;
        ResourcePool<GPUComputePipelineTag, SDL_GPUComputePipeline*> m_computePipelinePool;

        std::unique_ptr<IMipGenerator> m_mipGenerator;

        // ── Staging Ring Buffer ──────────────────────────────
        static constexpr uint32_t STAGING_RING_CAPACITY = 4 * 1024 * 1024; // 4 MB

        struct PendingBufferCopy
        {
            uint32_t ringOffset;
            uint32_t size;
            SDL_GPUBuffer* dstBuffer;
            uint32_t dstOffset;
        };

        struct PendingTextureCopy
        {
            uint32_t ringOffset;
            uint32_t size;
            SDL_GPUTexture* dstTexture;
            uint32_t width;
            uint32_t height;
            uint32_t pixelsPerRow;
            uint32_t rowsPerLayer;
            uint32_t mipLevel;
        };

        SDL_GPUTransferBuffer* m_stagingRing = nullptr;
        void* m_stagingMapped = nullptr;
        uint32_t m_stagingCursor = 0;

        std::vector<PendingBufferCopy> m_pendingBufferCopies;
        std::vector<PendingTextureCopy> m_pendingTextureCopies;

        void UploadToBufferDedicated(SDL_GPUBuffer* buffer, const void* data, BufferUploadRegion region);
        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        void UploadToTextureDedicated(SDL_GPUTexture* texture, const void* pixelData, uint32_t width, uint32_t height, uint32_t bytesPerRow, uint32_t mipLevel);

        [[nodiscard]] auto TryStageToRing(const void* data, uint32_t sizeInBytes) -> std::optional<uint32_t>;
    };

} // namespace Wayfinder
