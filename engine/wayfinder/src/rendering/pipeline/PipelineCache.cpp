#include "PipelineCache.h"
#include "core/Log.h"

#include <functional>

namespace Wayfinder
{
    void PipelineCache::Initialise(RenderDevice& device)
    {
        m_device = &device;
    }

    void PipelineCache::Shutdown()
    {
        if (m_device)
        {
            for (auto& [hash, handle] : m_cache)
            {
                if (handle)
                {
                    m_device->DestroyPipeline(handle);
                }
            }
        }

        m_cache.clear();
        m_device = nullptr;
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

        combine(std::hash<GPUShaderHandle>{}(desc.vertexShader));
        combine(std::hash<GPUShaderHandle>{}(desc.fragmentShader));
        combine(std::hash<uint32_t>{}(desc.vertexLayout.stride));
        combine(std::hash<uint32_t>{}(desc.vertexLayout.attribCount));

        for (uint32_t i = 0; i < desc.vertexLayout.attribCount; ++i)
        {
            const auto& a = desc.vertexLayout.attribs[i];
            combine(std::hash<uint32_t>{}(a.location));
            combine(std::hash<uint32_t>{}(a.offset));
            combine(std::hash<uint8_t>{}(static_cast<uint8_t>(a.format)));
        }

        combine(std::hash<uint8_t>{}(static_cast<uint8_t>(desc.primitiveType)));
        combine(std::hash<uint8_t>{}(static_cast<uint8_t>(desc.cullMode)));
        combine(std::hash<uint8_t>{}(static_cast<uint8_t>(desc.fillMode)));
        combine(std::hash<uint8_t>{}(static_cast<uint8_t>(desc.frontFace)));
        combine(std::hash<bool>{}(desc.depthTestEnabled));
        combine(std::hash<bool>{}(desc.depthWriteEnabled));

        // Colour target blend states and formats
        combine(std::hash<uint32_t>{}(desc.numColourTargets));
        for (uint32_t i = 0; i < desc.numColourTargets; ++i)
        {
            combine(std::hash<uint8_t>{}(static_cast<uint8_t>(desc.colourTargetFormats[i])));

            const auto& blend = desc.colourTargetBlends[i];
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

} // namespace Wayfinder
