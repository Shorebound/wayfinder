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
                return sizeof(Float4);
            case MaterialParamType::Colour:
                return sizeof(Float4); // LinearColour = float4
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
        pipelineDesc.primitiveType = PrimitiveType::TriangleList;
        pipelineDesc.colourTargetBlends.front() = desc.Blend;

        const GPUPipelineHandle pipeline = m_cache->GetOrCreate(*m_shaders, pipelineDesc);
        if (!pipeline.IsValid())
        {
            WAYFINDER_ERROR(LogRenderer, "ShaderProgramRegistry: Failed to create pipeline for '{}'", desc.Name);
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
        m_variantPipelines.clear();
        WAYFINDER_INFO(LogRenderer, "ShaderProgramRegistry: all programs invalidated");
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

        // Compute a cache key from program name hash + topology.
        size_t key = std::hash<std::string_view>{}(name);
        key ^= static_cast<size_t>(topology) + 0x9e3779b9 + (key << 6) + (key >> 2);

        if (const auto it = m_variantPipelines.find(key); it != m_variantPipelines.end())
        {
            return it->second;
        }

        // Build a new pipeline desc with the requested topology.
        GPUPipelineDesc pipelineDesc{};
        pipelineDesc.vertexShaderName = program->Desc.VertexShaderName;
        pipelineDesc.fragmentShaderName = program->Desc.FragmentShaderName;
        pipelineDesc.vertexResources = program->Desc.VertexResources;
        pipelineDesc.fragmentResources = program->Desc.FragmentResources;
        pipelineDesc.vertexLayout = program->Desc.VertexLayout;
        pipelineDesc.cullMode = program->Desc.Cull;
        pipelineDesc.depthTestEnabled = program->Desc.DepthTest;
        pipelineDesc.depthWriteEnabled = program->Desc.DepthWrite;
        pipelineDesc.primitiveType = topology;
        pipelineDesc.colourTargetBlends.front() = program->Desc.Blend;

        const GPUPipelineHandle handle = m_cache->GetOrCreate(*m_shaders, pipelineDesc);
        if (handle.IsValid())
        {
            m_variantPipelines[key] = handle;
        }
        return handle;
    }

} // namespace Wayfinder
