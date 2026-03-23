#include "ShaderProgram.h"

#include "ShaderManager.h"
#include "core/Log.h"
#include "rendering/backend/GPUPipeline.h"
#include "rendering/pipeline/PipelineCache.h"

namespace Wayfinder
{
    ShaderProgramRegistry::~ShaderProgramRegistry()
    {
        Shutdown();
    }

    void ShaderProgramRegistry::Initialise(RenderDevice& device, ShaderManager& shaders, PipelineCache& cache)
    {
        m_device = &device;
        m_shaders = &shaders;
        m_cache = &cache;
    }

    void ShaderProgramRegistry::Shutdown()
    {
        for (auto* pipeline : m_ownedPipelines)
        {
            pipeline->Destroy();
            delete pipeline;
        }
        m_ownedPipelines.clear();
        m_programs.clear();
        m_device = nullptr;
        m_shaders = nullptr;
        m_cache = nullptr;
    }

    bool ShaderProgramRegistry::Register(const ShaderProgramDesc& desc)
    {
        if (!m_device || !m_shaders || !m_cache)
        {
            WAYFINDER_ERROR(LogRenderer, "ShaderProgramRegistry: Cannot register '{}' — not initialised", desc.Name);
            return false;
        }

        if (m_programs.contains(desc.Name))
        {
            WAYFINDER_WARNING(LogRenderer, "ShaderProgramRegistry: '{}' already registered — skipping", desc.Name);
            return true;
        }

        GPUPipelineDesc pipelineDesc{};
        pipelineDesc.vertexShaderName = desc.VertexShaderName;
        pipelineDesc.fragmentShaderName = desc.FragmentShaderName;
        pipelineDesc.vertexResources = desc.VertexResources;
        pipelineDesc.fragmentResources = desc.FragmentResources;
        pipelineDesc.vertexLayout = desc.VertexLayout;
        pipelineDesc.cullMode = desc.Cull;
        pipelineDesc.depthTestEnabled = desc.DepthTest;
        pipelineDesc.depthWriteEnabled = desc.DepthWrite;
        pipelineDesc.colourTargetBlends[0] = desc.Blend;

        auto* pipeline = new GPUPipeline();
        if (!pipeline->Create(*m_device, *m_shaders, pipelineDesc, m_cache))
        {
            WAYFINDER_ERROR(LogRenderer, "ShaderProgramRegistry: Failed to create pipeline for '{}'", desc.Name);
            delete pipeline;
            return false;
        }

        m_ownedPipelines.push_back(pipeline);

        ShaderProgram program;
        program.Desc = desc;
        program.Pipeline = pipeline;
        m_programs.emplace(desc.Name, std::move(program));

        WAYFINDER_INFO(LogRenderer, "ShaderProgramRegistry: Registered '{}'", desc.Name);
        return true;
    }

    const ShaderProgram* ShaderProgramRegistry::Find(const std::string& name) const
    {
        auto it = m_programs.find(name);
        return (it != m_programs.end()) ? &it->second : nullptr;
    }

    const ShaderProgram* ShaderProgramRegistry::FindOrDefault(const std::string& name, const std::string& fallback) const
    {
        if (const auto* program = Find(name)) return program;
        if (const auto* fb = Find(fallback)) return fb;
        return nullptr;
    }

} // namespace Wayfinder
