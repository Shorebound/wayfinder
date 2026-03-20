#pragma once

#include "RenderDevice.h"

#include <unordered_map>

namespace Wayfinder
{
    // Caches GPU pipeline handles by configuration hash.
    // Prevents duplicate pipeline objects for identical PipelineCreateDesc configurations.
    // The cache owns all pipeline handles and destroys them on Shutdown.
    class WAYFINDER_API PipelineCache
    {
    public:
        PipelineCache() = default;
        ~PipelineCache() = default;

        PipelineCache(const PipelineCache&) = delete;
        PipelineCache& operator=(const PipelineCache&) = delete;

        void Initialise(RenderDevice& device);
        void Shutdown();

        // Returns an existing pipeline for this configuration, or creates and caches a new one.
        // The cache owns the returned handle — callers must not destroy it.
        GPUPipelineHandle GetOrCreate(const PipelineCreateDesc& desc);

    private:
        static size_t HashDesc(const PipelineCreateDesc& desc);

        RenderDevice* m_device = nullptr;
        std::unordered_map<size_t, GPUPipelineHandle> m_cache;
    };

} // namespace Wayfinder
