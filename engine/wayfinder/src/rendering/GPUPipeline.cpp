#include "GPUPipeline.h"
#include "PipelineCache.h"
#include "ShaderManager.h"
#include "../core/Log.h"

namespace Wayfinder
{
    bool GPUPipeline::Create(RenderDevice& device, ShaderManager& shaders, const GPUPipelineDesc& desc, PipelineCache* cache)
    {
        m_device = &device;

        GPUShaderHandle vs = shaders.GetShader(desc.vertexShaderName, ShaderStage::Vertex, desc.vertexResources);
        GPUShaderHandle fs = shaders.GetShader(desc.fragmentShaderName, ShaderStage::Fragment, desc.fragmentResources);
        if (!vs || !fs)
        {
            WAYFINDER_ERROR(LogRenderer, "GPUPipeline: Failed to resolve shaders '{}' / '{}'",
                desc.vertexShaderName, desc.fragmentShaderName);
            return false;
        }

        if (desc.numColourTargets == 0 || desc.numColourTargets > MAX_COLOUR_TARGETS)
        {
            WAYFINDER_ERROR(LogRenderer, "GPUPipeline: numColourTargets={} is out of range [1, {}]",
                desc.numColourTargets, MAX_COLOUR_TARGETS);
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
        pipeDesc.numColourTargets = desc.numColourTargets;
        for (uint32_t i = 0; i < desc.numColourTargets; ++i)
        {
            pipeDesc.colourTargetBlends[i] = desc.colourTargetBlends[i];
        }

        if (cache)
        {
            m_pipeline = cache->GetOrCreate(pipeDesc);
            m_isFromCache = m_pipeline.IsValid();
        }
        else
        {
            m_pipeline = device.CreatePipeline(pipeDesc);
        }

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
        if (m_isFromCache)
        {
            m_pipeline = {};
            m_isFromCache = false;
            return;
        }

        if (m_device && m_pipeline.IsValid())
        {
            m_device->DestroyPipeline(m_pipeline);
            m_pipeline = {};
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
