#pragma once

#include "../RenderDevice.h"

struct SDL_Window;
struct SDL_GPUDevice;
struct SDL_GPUCommandBuffer;
struct SDL_GPURenderPass;
struct SDL_GPUTexture;

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
    };

} // namespace Wayfinder
