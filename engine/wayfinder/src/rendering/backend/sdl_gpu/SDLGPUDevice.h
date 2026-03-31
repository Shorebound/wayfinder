#pragma once

#include "core/ResourcePool.h"
#include "rendering/backend/RenderDevice.h"

#include <optional>

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
    class IMipGenerator;

    /** @brief Metadata stored alongside each SDL texture in the resource pool. */
    struct TextureEntry
    {
        SDL_GPUTexture* Texture = nullptr;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t MipLevels = 1;
    };

    class SDLGPUDevice final : public RenderDevice
    {
    public:
        SDLGPUDevice() = default;
        ~SDLGPUDevice() noexcept override;

        Result<void> Initialise(Window& window) override;
        void Shutdown() override;

        bool BeginFrame() override;
        void EndFrame() override;

        void PushDebugGroup(std::string_view name) override;
        void PopDebugGroup() override;

        bool BeginRenderPass(const RenderPassDescriptor& descriptor) override;
        void EndRenderPass() override;

        GPUShaderHandle CreateShader(const ShaderCreateDesc& desc) override;
        void DestroyShader(GPUShaderHandle shader) override;

        GPUPipelineHandle CreatePipeline(const PipelineCreateDesc& desc) override;
        void DestroyPipeline(GPUPipelineHandle pipeline) override;

        void BindPipeline(GPUPipelineHandle pipeline) override;

        GPUBufferHandle CreateBuffer(const BufferCreateDesc& desc) override;
        void DestroyBuffer(GPUBufferHandle buffer) override;
        void UploadToBuffer(GPUBufferHandle buffer, const void* data, BufferUploadRegion region) override;
        void FlushUploads() override;

        void BindVertexBuffer(GPUBufferHandle buffer, VertexBufferBindingDesc binding = {}) override;
        void BindIndexBuffer(GPUBufferHandle buffer, IndexElementSize indexSize, uint32_t offsetInBytes = 0) override;
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0) override;
        void DrawPrimitives(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0) override;
        void PushVertexUniform(uint32_t slot, const void* data, uint32_t sizeInBytes) override;
        void PushFragmentUniform(uint32_t slot, const void* data, uint32_t sizeInBytes) override;

        GPUComputePipelineHandle CreateComputePipeline(const ComputePipelineCreateDesc& desc) override;
        void DestroyComputePipeline(GPUComputePipelineHandle pipeline) override;
        void BeginComputePass() override;
        void EndComputePass() override;
        void BindComputePipeline(GPUComputePipelineHandle pipeline) override;
        void DispatchCompute(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;

        GPUTextureHandle CreateTexture(const TextureCreateDesc& desc) override;
        void DestroyTexture(GPUTextureHandle texture) override;
        void UploadToTexture(GPUTextureHandle texture, const void* pixelData, uint32_t width, uint32_t height, uint32_t bytesPerRow, uint32_t mipLevel = 0) override;
        void GenerateMipmaps(GPUTextureHandle texture) override;

        GPUSamplerHandle CreateSampler(const SamplerCreateDesc& desc) override;
        void DestroySampler(GPUSamplerHandle sampler) override;

        void BindFragmentSampler(uint32_t slot, GPUTextureHandle texture, GPUSamplerHandle sampler) override;

        [[nodiscard]] Extent2D GetSwapchainDimensions() const override;

        const RenderDeviceInfo& GetDeviceInfo() const override
        {
            return m_info;
        }

        SDL_GPUDevice* GetGPUDevice() const
        {
            return m_device;
        }

    private:
        SDL_GPUDevice* m_device = nullptr;
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

        // Depth buffer — owned by device, matched to swapchain dimensions
        SDL_GPUTexture* m_depthTexture = nullptr;
        uint32_t m_depthWidth = 0;
        uint32_t m_depthHeight = 0;
        void EnsureDepthTexture(uint32_t width, uint32_t height);

        // Shader format accepted by the chosen backend (queried at init)
        uint32_t m_shaderFormats = 0;

        // ── Resource Pools ───────────────────────────────────
        // Raw SDL pointers never leave the backend — the pools map
        // generational handles to the underlying GPU objects.
        ResourcePool<GPUShaderTag, SDL_GPUShader*> m_shaderPool;
        ResourcePool<GPUPipelineTag, SDL_GPUGraphicsPipeline*> m_pipelinePool;
        ResourcePool<GPUBufferTag, SDL_GPUBuffer*> m_bufferPool;
        ResourcePool<GPUTextureTag, TextureEntry> m_texturePool;
        ResourcePool<GPUSamplerTag, SDL_GPUSampler*> m_samplerPool;
        ResourcePool<GPUComputePipelineTag, SDL_GPUComputePipeline*> m_computePipelinePool;

        std::unique_ptr<IMipGenerator> m_mipGenerator;

        // ── Staging Ring Buffer ──────────────────────────────
        // Batches UploadToBuffer / UploadToTexture into one copy pass.
        /// @note Not thread-safe — all staging and flush calls must happen on the
        /// render thread. If multi-threaded upload is needed later, the ring
        /// will need per-thread sub-allocators or a lock.
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
        void UploadToTextureDedicated(SDL_GPUTexture* texture, const void* pixelData, uint32_t width, uint32_t height, uint32_t bytesPerRow, uint32_t mipLevel);

        /// Try to append data into the staging ring. Returns the ring offset on
        /// success, or std::nullopt if the data doesn't fit (even after one flush).
        std::optional<uint32_t> TryStageToRing(const void* data, uint32_t sizeInBytes);
    };

} // namespace Wayfinder
