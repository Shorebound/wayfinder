#include "GPUPipeline.h"
#include "ShaderManager.h"
#include "../core/Log.h"

namespace Wayfinder
{
    bool GPUPipeline::Create(RenderDevice& device, ShaderManager& shaders, const GPUPipelineDesc& desc)
    {
        m_device = &device;

        GPUShaderHandle vs = shaders.GetShader(desc.vertexShaderName, ShaderStage::Vertex);
        GPUShaderHandle fs = shaders.GetShader(desc.fragmentShaderName, ShaderStage::Fragment);
        if (!vs || !fs)
        {
            WAYFINDER_ERROR(LogRenderer, "GPUPipeline: Failed to resolve shaders '{}' / '{}'",
                desc.vertexShaderName, desc.fragmentShaderName);
            return false;
        }

        PipelineCreateDesc pipeDesc{};
        pipeDesc.vertexShader = vs;
        pipeDesc.fragmentShader = fs;
        pipeDesc.vertexLayout = desc.vertexLayout;
        pipeDesc.primitiveType = desc.primitiveType;
        pipeDesc.cullMode = desc.cullMode;
        pipeDesc.fillMode = desc.fillMode;
        pipeDesc.frontFace = desc.frontFace;
        pipeDesc.depthTestEnabled = desc.depthTestEnabled;
        pipeDesc.depthWriteEnabled = desc.depthWriteEnabled;

        m_pipeline = device.CreatePipeline(pipeDesc);
        if (!m_pipeline)
        {
            WAYFINDER_ERROR(LogRenderer, "GPUPipeline: Pipeline creation failed");
            return false;
        }

        WAYFINDER_INFO(LogRenderer, "GPUPipeline: Created pipeline (vs='{}', fs='{}')",
            desc.vertexShaderName, desc.fragmentShaderName);
        return true;
    }

    void GPUPipeline::Destroy()
    {
        if (m_device && m_pipeline)
        {
            m_device->DestroyPipeline(m_pipeline);
            m_pipeline = nullptr;
        }
    }

    void GPUPipeline::Bind()
    {
        if (m_device && m_pipeline)
        {
            m_device->BindPipeline(m_pipeline);
        }
    }

} // namespace Wayfinder
