#pragma once

#include "../RenderDevice.h"

struct SDL_Window;
struct SDL_GPUDevice;
struct SDL_GPUCommandBuffer;
struct SDL_GPURenderPass;
struct SDL_GPUTexture;
struct SDL_GPUComputePass;

namespace Wayfinder
{
    class SDLGPUDevice final : public RenderDevice
    {
    public:
        SDLGPUDevice() = default;
        ~SDLGPUDevice() override;

        bool Initialize(Window& window) override;
        void Shutdown() override;

        bool BeginFrame() override;
        void EndFrame() override;

        void BeginRenderPass(const RenderPassDescriptor& descriptor) override;
        void EndRenderPass() override;

        GPUShaderHandle CreateShader(const ShaderCreateDesc& desc) override;
        void DestroyShader(GPUShaderHandle shader) override;

        GPUPipelineHandle CreatePipeline(const PipelineCreateDesc& desc) override;
        void DestroyPipeline(GPUPipelineHandle pipeline) override;

        void BindPipeline(GPUPipelineHandle pipeline) override;

        GPUBufferHandle CreateBuffer(const BufferCreateDesc& desc) override;
        void DestroyBuffer(GPUBufferHandle buffer) override;
        void UploadToBuffer(GPUBufferHandle buffer, const void* data, uint32_t sizeInBytes, uint32_t dstOffsetInBytes = 0) override;

        void BindVertexBuffer(GPUBufferHandle buffer, uint32_t slot = 0, uint32_t offsetInBytes = 0) override;
        void BindIndexBuffer(GPUBufferHandle buffer, IndexElementSize indexSize, uint32_t offsetInBytes = 0) override;
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1,
                         uint32_t firstIndex = 0, int32_t vertexOffset = 0) override;
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

        GPUSamplerHandle CreateSampler(const SamplerCreateDesc& desc) override;
        void DestroySampler(GPUSamplerHandle sampler) override;

        void BindFragmentSampler(uint32_t slot, GPUTextureHandle texture, GPUSamplerHandle sampler) override;

        void GetSwapchainDimensions(uint32_t& width, uint32_t& height) const override;

        const RenderDeviceInfo& GetDeviceInfo() const override { return m_info; }

        SDL_GPUDevice* GetGPUDevice() const { return m_device; }

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
    };

} // namespace Wayfinder
