#include "PipelineCache.h"
#include "core/Log.h"
#include "rendering/materials/ShaderManager.h"

#include <algorithm>
#include <functional>
#include <ranges>

namespace Wayfinder
{
    void PipelineCache::Initialise(RenderDevice& device)
    {
        m_device = &device;
    }

    void PipelineCache::Shutdown()
    {
        InvalidateAll();
        m_device = nullptr;
    }

    void PipelineCache::InvalidateAll()
    {
        if (m_device)
        {
            for (auto& handle : m_cache | std::views::values)
            {
                if (handle)
                {
                    m_device->DestroyPipeline(handle);
                }
            }
        }

        m_cache.clear();
    }

    GPUPipelineHandle PipelineCache::GetOrCreate(const PipelineCreateDesc& desc)
    {
        if (!m_device)
        {
            return GPUPipelineHandle::Invalid();
        }

        const size_t hash = HashDesc(desc);
        auto it = m_cache.find(hash);
        if (it != m_cache.end())
        {
            return it->second;
        }

        GPUPipelineHandle handle = m_device->CreatePipeline(desc);
        if (handle.IsValid())
        {
            m_cache[hash] = handle;
            WAYFINDER_INFO(LogRenderer, "PipelineCache: Cached new pipeline (hash={:#x}, total={})", hash, m_cache.size());
        }

        return handle;
    }

    size_t PipelineCache::HashDesc(const PipelineCreateDesc& desc)
    {
        size_t h = 0;
        auto combine = [&h](size_t v)
        {
            h ^= v + 0x9e3779b9 + (h << 6) + (h >> 2);
        };

        combine(std::hash<GPUShaderHandle>{}(desc.VertexShader));
        combine(std::hash<GPUShaderHandle>{}(desc.FragmentShader));
        combine(std::hash<uint32_t>{}(desc.VertexLayout.Stride));
        combine(std::hash<uint32_t>{}(desc.VertexLayout.AttributeCount));

        for (uint32_t i = 0; i < desc.VertexLayout.AttributeCount; ++i)
        {
            const auto& a = desc.VertexLayout.Attributes[i];
            combine(std::hash<uint32_t>{}(a.Location));
            combine(std::hash<uint32_t>{}(a.Offset));
            combine(std::hash<uint8_t>{}(static_cast<uint8_t>(a.Format)));
        }

        combine(std::hash<uint8_t>{}(static_cast<uint8_t>(desc.PrimitiveType)));
        combine(std::hash<uint8_t>{}(static_cast<uint8_t>(desc.CullMode)));
        combine(std::hash<uint8_t>{}(static_cast<uint8_t>(desc.FillMode)));
        combine(std::hash<uint8_t>{}(static_cast<uint8_t>(desc.FrontFace)));
        combine(std::hash<bool>{}(desc.DepthTestEnabled));
        combine(std::hash<bool>{}(desc.DepthWriteEnabled));

        // Colour target blend states and formats
        const uint32_t numTargets = std::min(desc.ColourTargets, static_cast<uint32_t>(MAX_COLOUR_TARGETS));
        combine(std::hash<uint32_t>{}(numTargets));
        for (uint32_t i = 0; i < numTargets; ++i)
        {
            combine(std::hash<uint8_t>{}(static_cast<uint8_t>(desc.ColourTargetFormats[i])));

            const auto& blend = desc.ColourTargetBlends[i];
            combine(std::hash<bool>{}(blend.Enabled));
            combine(std::hash<uint8_t>{}(static_cast<uint8_t>(blend.SrcColourFactor)));
            combine(std::hash<uint8_t>{}(static_cast<uint8_t>(blend.DstColourFactor)));
            combine(std::hash<uint8_t>{}(static_cast<uint8_t>(blend.ColourOp)));
            combine(std::hash<uint8_t>{}(static_cast<uint8_t>(blend.SrcAlphaFactor)));
            combine(std::hash<uint8_t>{}(static_cast<uint8_t>(blend.DstAlphaFactor)));
            combine(std::hash<uint8_t>{}(static_cast<uint8_t>(blend.AlphaOp)));
            combine(std::hash<uint8_t>{}(blend.ColourWriteMask));
        }

        return h;
    }

    GPUPipelineHandle PipelineCache::GetOrCreate(ShaderManager& shaders, const GPUPipelineDesc& desc)
    {
        if (!m_device)
        {
            return GPUPipelineHandle::Invalid();
        }

        const GPUShaderHandle vs = shaders.GetShader(desc.vertexShaderName, ShaderStage::Vertex, desc.vertexResources);
        const GPUShaderHandle fs = shaders.GetShader(desc.fragmentShaderName, ShaderStage::Fragment, desc.fragmentResources);
        if (!vs || !fs)
        {
            WAYFINDER_ERROR(LogRenderer, "PipelineCache: Failed to resolve shaders '{}' / '{}'", desc.vertexShaderName, desc.fragmentShaderName);
            return GPUPipelineHandle::Invalid();
        }

        if (desc.numColourTargets == 0 || desc.numColourTargets > MAX_COLOUR_TARGETS)
        {
            WAYFINDER_ERROR(LogRenderer, "PipelineCache: numColourTargets={} is out of range [1, {}]", desc.numColourTargets, MAX_COLOUR_TARGETS);
            return GPUPipelineHandle::Invalid();
        }

        PipelineCreateDesc pipeDesc{};
        pipeDesc.VertexShader = vs;
        pipeDesc.FragmentShader = fs;
        pipeDesc.VertexLayout = desc.vertexLayout;
        pipeDesc.PrimitiveType = desc.primitiveType;
        pipeDesc.CullMode = desc.cullMode;
        pipeDesc.FillMode = desc.fillMode;
        pipeDesc.FrontFace = desc.frontFace;
        pipeDesc.DepthTestEnabled = desc.depthTestEnabled;
        pipeDesc.DepthWriteEnabled = desc.depthWriteEnabled;
        pipeDesc.ColourTargets = desc.numColourTargets;
        std::copy_n(desc.colourTargetFormats.begin(), desc.numColourTargets, pipeDesc.ColourTargetFormats.begin());
        std::copy_n(desc.colourTargetBlends.begin(), desc.numColourTargets, pipeDesc.ColourTargetBlends.begin());

        return GetOrCreate(pipeDesc);
    }

} // namespace Wayfinder
