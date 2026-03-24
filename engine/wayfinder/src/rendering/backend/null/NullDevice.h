#pragma once

#include "rendering/backend/RenderDevice.h"

namespace Wayfinder
{
    class NullDevice final : public RenderDevice
    {
    public:
        NullDevice() = default;
        explicit NullDevice(const std::string& backendName)
        {
            m_info.BackendName = backendName;
        }
        ~NullDevice() override = default;

        Result<void> Initialise(Window&) override
        {
            return {};
        }
        void Shutdown() override {}

        bool BeginFrame() override
        {
            return true;
        }
        void EndFrame() override {}

        void BeginRenderPass(const RenderPassDescriptor&) override {}
        void EndRenderPass() override {}

        // ── Shader / Pipeline (no-ops) ──
        GPUShaderHandle CreateShader(const ShaderCreateDesc&) override
        {
            return {};
        }
        void DestroyShader(GPUShaderHandle) override {}
        GPUPipelineHandle CreatePipeline(const PipelineCreateDesc&) override
        {
            return {};
        }
        void DestroyPipeline(GPUPipelineHandle) override {}
        void BindPipeline(GPUPipelineHandle) override {}

        // ── Buffer / Draw (no-ops) ──
        GPUBufferHandle CreateBuffer(const BufferCreateDesc&) override
        {
            return {};
        }
        void DestroyBuffer(GPUBufferHandle) override {}
        void UploadToBuffer(GPUBufferHandle, const void*, BufferUploadRegion) override {}
        void BindVertexBuffer(GPUBufferHandle, VertexBufferBindingDesc) override {}
        void BindIndexBuffer(GPUBufferHandle, IndexElementSize, uint32_t) override {}
        void DrawIndexed(uint32_t, uint32_t, uint32_t, int32_t) override {}
        void DrawPrimitives(uint32_t, uint32_t, uint32_t) override {}
        void PushVertexUniform(uint32_t, const void*, uint32_t) override {}
        void PushFragmentUniform(uint32_t, const void*, uint32_t) override {}

        // ── Compute (no-ops) ──
        GPUComputePipelineHandle CreateComputePipeline(const ComputePipelineCreateDesc&) override
        {
            return {};
        }
        void DestroyComputePipeline(GPUComputePipelineHandle) override {}
        void BeginComputePass() override {}
        void EndComputePass() override {}
        void BindComputePipeline(GPUComputePipelineHandle) override {}
        void DispatchCompute(uint32_t, uint32_t, uint32_t) override {}

        // ── Textures (no-ops) ──
        GPUTextureHandle CreateTexture(const TextureCreateDesc&) override
        {
            return {};
        }
        void DestroyTexture(GPUTextureHandle) override {}
        void UploadToTexture(GPUTextureHandle, const void*, uint32_t, uint32_t, uint32_t) override {}
        void UploadToTexture(GPUTextureHandle, const void*, uint32_t, uint32_t, uint32_t, uint32_t) override {}
        void GenerateMipmaps(GPUTextureHandle, uint32_t, uint32_t, uint32_t) override {}

        // ── Samplers (no-ops) ──
        GPUSamplerHandle CreateSampler(const SamplerCreateDesc&) override
        {
            return {};
        }
        void DestroySampler(GPUSamplerHandle) override {}
        void BindFragmentSampler(uint32_t, GPUTextureHandle, GPUSamplerHandle) override {}

        [[nodiscard]] Extent2D GetSwapchainDimensions() const override
        {
            return {};
        }

        const RenderDeviceInfo& GetDeviceInfo() const override
        {
            return m_info;
        }

    private:
        RenderDeviceInfo m_info{.BackendName = "Null"};
    };

} // namespace Wayfinder
