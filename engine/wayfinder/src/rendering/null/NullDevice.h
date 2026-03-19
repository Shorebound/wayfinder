#pragma once

#include "../RenderDevice.h"

namespace Wayfinder
{
    class NullDevice final : public RenderDevice
    {
    public:
        NullDevice() = default;
        explicit NullDevice(const std::string& backendName) { m_info.BackendName = backendName; }
        ~NullDevice() override = default;

        bool Initialize(Window&) override { return true; }
        void Shutdown() override {}

        bool BeginFrame() override { return true; }
        void EndFrame() override {}

        void BeginRenderPass(const RenderPassDescriptor&) override {}
        void EndRenderPass() override {}

        // ── Shader / Pipeline (no-ops) ──
        GPUShaderHandle   CreateShader(const ShaderCreateDesc&) override { return nullptr; }
        void              DestroyShader(GPUShaderHandle) override {}
        GPUPipelineHandle CreatePipeline(const PipelineCreateDesc&) override { return nullptr; }
        void              DestroyPipeline(GPUPipelineHandle) override {}
        void              BindPipeline(GPUPipelineHandle) override {}

        // ── Buffer / Draw (no-ops) ──
        GPUBufferHandle   CreateBuffer(const BufferCreateDesc&) override { return nullptr; }
        void              DestroyBuffer(GPUBufferHandle) override {}
        void              UploadToBuffer(GPUBufferHandle, const void*, uint32_t, uint32_t) override {}
        void              BindVertexBuffer(GPUBufferHandle, uint32_t, uint32_t) override {}
        void              BindIndexBuffer(GPUBufferHandle, IndexElementSize, uint32_t) override {}
        void              DrawIndexed(uint32_t, uint32_t, uint32_t, int32_t) override {}
        void              DrawPrimitives(uint32_t, uint32_t, uint32_t) override {}
        void              PushVertexUniform(uint32_t, const void*, uint32_t) override {}
        void              PushFragmentUniform(uint32_t, const void*, uint32_t) override {}

        // ── Compute (no-ops) ──
        GPUComputePipelineHandle CreateComputePipeline(const ComputePipelineCreateDesc&) override { return nullptr; }
        void              DestroyComputePipeline(GPUComputePipelineHandle) override {}
        void              BeginComputePass() override {}
        void              EndComputePass() override {}
        void              BindComputePipeline(GPUComputePipelineHandle) override {}
        void              DispatchCompute(uint32_t, uint32_t, uint32_t) override {}

        const RenderDeviceInfo& GetDeviceInfo() const override { return m_info; }

    private:
        RenderDeviceInfo m_info{.BackendName = "Null"};
    };

} // namespace Wayfinder
