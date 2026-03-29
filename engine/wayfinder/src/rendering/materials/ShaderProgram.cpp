#include "ShaderProgram.h"

#include "ShaderManager.h"
#include "core/Log.h"

#include "rendering/pipeline/PipelineCache.h"
#include <string>

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
            WAYFINDER_WARN(LogRenderer, "ShaderProgramRegistry: '{}' already registered — skipping", desc.Name);
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
        pipelineDesc.colourTargetBlends.front() = desc.Blend;

        const GPUPipelineHandle pipeline = m_cache->GetOrCreate(*m_shaders, pipelineDesc);
        if (!pipeline.IsValid())
        {
            WAYFINDER_ERROR(LogRenderer, "ShaderProgramRegistry: Failed to create pipeline for '{}'", desc.Name);
            return false;
        }

        ShaderProgram program;
        program.Desc = desc;
        program.Pipeline = pipeline;
        m_programs.emplace(desc.Name, std::move(program));

        WAYFINDER_INFO(LogRenderer, "ShaderProgramRegistry: Registered '{}'", desc.Name);
        return true;
    }

    const ShaderProgram* ShaderProgramRegistry::Find(const std::string_view name) const
    {
        const auto it = m_programs.find(name);
        return (it != m_programs.end()) ? &it->second : nullptr;
    }

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — primary name vs fallback shader name
    const ShaderProgram* ShaderProgramRegistry::FindOrDefault(const std::string_view name, const std::string_view fallback) const
    {
        if (const auto* program = Find(name))
        {
            return program;
        }
        if (const auto* fb = Find(fallback))
        {
            return fb;
        }
        return nullptr;
    }

    void ShaderProgramRegistry::InvalidateAll()
    {
        // Pipeline handles are owned by PipelineCache (invalidated separately).
        // Just clear the program map so Register() can recreate entries.
        m_programs.clear();
        WAYFINDER_INFO(LogRenderer, "ShaderProgramRegistry: all programs invalidated");
    }

} // namespace Wayfinder
