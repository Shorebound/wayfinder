#include "ShaderProgram.h"

#include "ShaderManager.h"
#include "core/Log.h"

#include "rendering/pipeline/PipelineCache.h"
#include <algorithm>
#include <string>

namespace Wayfinder
{
    namespace
    {
        /// Returns the byte size of a single material parameter type.
        uint32_t ParamTypeSize(MaterialParamType type)
        {
            switch (type)
            {
            case MaterialParamType::Float:
                return sizeof(float);
            case MaterialParamType::Vec2:
                return sizeof(Float2);
            case MaterialParamType::Vec3:
                return sizeof(Float3);
            case MaterialParamType::Vec4:
            case MaterialParamType::Colour: // LinearColour = float4
                return sizeof(Float4);
            case MaterialParamType::Int:
                return sizeof(int32_t);
            }
            return 0;
        }

        /// Computes the minimum UBO size required to hold all declared parameters.
        /// Returns the byte past the last parameter (offset + size), rounded up to
        /// 16-byte alignment (std140 UBO requirement).
        uint32_t ComputeMaterialUBOSize(const std::vector<MaterialParamDecl>& params)
        {
            if (params.empty())
            {
                return 0;
            }

            uint32_t maxEnd = 0;
            for (const auto& p : params)
            {
                const uint32_t end = p.Offset + ParamTypeSize(p.Type);
                maxEnd = std::max(maxEnd, end);
            }

            // Round up to 16-byte alignment (std140 UBO granularity).
            return (maxEnd + 15u) & ~15u;
        }
    } // namespace

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
            Log::Error(LogRenderer, "ShaderProgramRegistry: Cannot register '{}' — not initialised", desc.Name);
            return false;
        }

        if (m_programs.contains(desc.Name))
        {
            Log::Warn(LogRenderer, "ShaderProgramRegistry: '{}' already registered — skipping", desc.Name);
            return true;
        }

        GPUPipelineDesc pipelineDesc{};
        pipelineDesc.VertexShaderName = desc.VertexShaderName;
        pipelineDesc.FragmentShaderName = desc.FragmentShaderName;
        pipelineDesc.VertexResources = desc.VertexResources;
        pipelineDesc.FragmentResources = desc.FragmentResources;
        pipelineDesc.VertexLayout = desc.VertexLayout;
        pipelineDesc.CullMode = desc.Cull;
        pipelineDesc.DepthTestEnabled = desc.DepthTest;
        pipelineDesc.DepthWriteEnabled = desc.DepthWrite;
        pipelineDesc.PrimitiveType = PrimitiveType::TriangleList;
        pipelineDesc.ColourTargetBlends.front() = desc.Blend;

        const GPUPipelineHandle pipeline = m_cache->GetOrCreate(*m_shaders, pipelineDesc);
        if (!pipeline.IsValid())
        {
            Log::Error(LogRenderer, "ShaderProgramRegistry: Failed to create pipeline for '{}'", desc.Name);
            return false;
        }

        ShaderProgram program;
        program.Desc = desc;

        // Auto-compute MaterialUBOSize from parameter declarations if not explicitly set.
        if (program.Desc.MaterialUBOSize == 0 && !program.Desc.MaterialParams.empty())
        {
            program.Desc.MaterialUBOSize = ComputeMaterialUBOSize(program.Desc.MaterialParams);
        }

        program.Pipeline = pipeline;
        m_programs.emplace(desc.Name, std::move(program));

        Log::Info(LogRenderer, "ShaderProgramRegistry: Registered '{}'", desc.Name);
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
        m_variantPipelines.clear();
        Log::Info(LogRenderer, "ShaderProgramRegistry: all programs invalidated");
    }

    GPUPipelineHandle ShaderProgramRegistry::GetVariantPipeline(const std::string_view name, const PrimitiveType topology)
    {
        const auto* program = Find(name);
        if (!program || !m_cache || !m_shaders)
        {
            return GPUPipelineHandle::Invalid();
        }

        // For TriangleList, the default pipeline already matches.
        if (topology == PrimitiveType::TriangleList)
        {
            return program->Pipeline;
        }

        // Look up or create a variant pipeline for this (program, topology) pair.
        const VariantKey key{.Name = std::string(name), .Topology = topology};

        if (const auto it = m_variantPipelines.find(key); it != m_variantPipelines.end())
        {
            return it->second;
        }

        // Build a new pipeline desc with the requested topology.
        GPUPipelineDesc pipelineDesc{};
        pipelineDesc.VertexShaderName = program->Desc.VertexShaderName;
        pipelineDesc.FragmentShaderName = program->Desc.FragmentShaderName;
        pipelineDesc.VertexResources = program->Desc.VertexResources;
        pipelineDesc.FragmentResources = program->Desc.FragmentResources;
        pipelineDesc.VertexLayout = program->Desc.VertexLayout;
        pipelineDesc.CullMode = program->Desc.Cull;
        pipelineDesc.DepthTestEnabled = program->Desc.DepthTest;
        pipelineDesc.DepthWriteEnabled = program->Desc.DepthWrite;
        pipelineDesc.PrimitiveType = topology;
        pipelineDesc.ColourTargetBlends.front() = program->Desc.Blend;

        const GPUPipelineHandle handle = m_cache->GetOrCreate(*m_shaders, pipelineDesc);
        if (handle.IsValid())
        {
            m_variantPipelines[key] = handle;
        }
        return handle;
    }

} // namespace Wayfinder
