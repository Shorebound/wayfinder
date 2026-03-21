#include "RenderGraph.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/resources/TransientResourcePool.h"
#include "core/Log.h"

#include <algorithm>
#include <queue>
#include <unordered_set>

namespace Wayfinder
{
    // ── Builder ──────────────────────────────────────────────

    RenderGraphBuilder::RenderGraphBuilder(RenderGraph& graph, uint32_t passIndex)
        : m_graph(graph), m_passIndex(passIndex) {}

    RenderGraphHandle RenderGraphBuilder::CreateTransient(const RenderGraphTextureDesc& desc)
    {
        return m_graph.AllocateResource(desc, desc.DebugName);
    }

    void RenderGraphBuilder::ReadTexture(RenderGraphHandle handle)
    {
        if (!handle.IsValid() || handle.Index >= m_graph.m_resources.size()) return;
        auto& pass = m_graph.m_passes[m_passIndex];
        pass.Reads.push_back(handle);

        auto& res = m_graph.m_resources[handle.Index];
        // Record dependency on whoever last wrote this resource
        if (res.WrittenByPass != UINT32_MAX && res.WrittenByPass != m_passIndex)
        {
            pass.DependsOn.push_back(res.WrittenByPass);
        }
        res.IsReadAsSampler = true;
        res.LastReadByPass = (res.LastReadByPass == UINT32_MAX) ? m_passIndex : std::max(res.LastReadByPass, m_passIndex);
    }

    void RenderGraphBuilder::WriteColour(RenderGraphHandle handle, LoadOp load, ClearValue clear)
    {
        if (!handle.IsValid() || handle.Index >= m_graph.m_resources.size()) return;
        auto& pass = m_graph.m_passes[m_passIndex];
        pass.ColourWrite = RenderGraph::ColourWriteInfo{handle, load, clear};

        auto& res = m_graph.m_resources[handle.Index];
        // LoadOp::Load implies reading previous content
        if (load == LoadOp::Load && res.WrittenByPass != UINT32_MAX && res.WrittenByPass != m_passIndex)
        {
            pass.DependsOn.push_back(res.WrittenByPass);
        }
        // Always track the last writer
        res.WrittenByPass = m_passIndex;
    }

    void RenderGraphBuilder::WriteDepth(RenderGraphHandle handle, LoadOp load, float clearDepth)
    {
        if (!handle.IsValid() || handle.Index >= m_graph.m_resources.size()) return;
        auto& pass = m_graph.m_passes[m_passIndex];
        pass.DepthWrite = RenderGraph::DepthWriteInfo{handle, load, clearDepth};

        auto& res = m_graph.m_resources[handle.Index];
        // LoadOp::Load implies reading previous content
        if (load == LoadOp::Load && res.WrittenByPass != UINT32_MAX && res.WrittenByPass != m_passIndex)
        {
            pass.DependsOn.push_back(res.WrittenByPass);
        }
        // Always track the last writer
        res.WrittenByPass = m_passIndex;
    }

    void RenderGraphBuilder::SetSwapchainOutput(LoadOp load, ClearValue clear)
    {
        auto& pass = m_graph.m_passes[m_passIndex];
        pass.SwapchainWrite = RenderGraph::SwapchainWriteInfo{load, clear};
    }

    // ── Resources ────────────────────────────────────────────

    GPUTextureHandle RenderGraphResources::GetTexture(RenderGraphHandle handle) const
    {
        if (!handle.IsValid() || handle.Index >= m_textures.size())
        {
            WAYFINDER_VERBOSE(LogRenderer, "RenderGraphResources::GetTexture: invalid handle (index={}, valid={}, count={})",
                            handle.Index, handle.IsValid(), m_textures.size());
            return GPUTextureHandle::Invalid();
        }
        return m_textures[handle.Index];
    }

    // ── RenderGraph ──────────────────────────────────────────

    RenderGraphHandle RenderGraph::AllocateResource(const RenderGraphTextureDesc& desc, const std::string& name)
    {
        RenderGraphHandle handle;
        handle.Index = static_cast<uint32_t>(m_resources.size());

        ResourceEntry entry;
        entry.Desc = desc;
        entry.Name = name;
        m_resources.push_back(std::move(entry));

        return handle;
    }

    void RenderGraph::AddPass(const std::string& name, PassSetupFn setup)
    {
        uint32_t passIndex = static_cast<uint32_t>(m_passes.size());
        m_passes.push_back({});
        m_passes.back().Name = name;
        m_passes.back().Type = RenderGraphPassType::Raster;

        RenderGraphBuilder builder(*this, passIndex);
        m_passes.back().Execute = setup(builder);
    }

    void RenderGraph::AddComputePass(const std::string& name, PassSetupFn setup)
    {
        uint32_t passIndex = static_cast<uint32_t>(m_passes.size());
        m_passes.push_back({});
        m_passes.back().Name = name;
        m_passes.back().Type = RenderGraphPassType::Compute;

        RenderGraphBuilder builder(*this, passIndex);
        m_passes.back().Execute = setup(builder);
    }

    RenderGraphHandle RenderGraph::ImportTexture(const std::string& name)
    {
        // Check if already imported
        for (uint32_t i = 0; i < m_resources.size(); ++i)
        {
            if (m_resources[i].Name == name)
            {
                return {i};
            }
        }

        // Imported/externally-provided textures use a default-constructed desc
        // (width/height == 0) so the transient allocation step skips them —
        // their GPU handles are supplied externally before Execute().
        RenderGraphTextureDesc desc;
        return AllocateResource(desc, name);
    }

    RenderGraphHandle RenderGraph::FindHandle(const std::string& name) const
    {
        for (uint32_t i = 0; i < m_resources.size(); ++i)
        {
            if (m_resources[i].Name == name)
            {
                return {i};
            }
        }
        return {};
    }

    bool RenderGraph::Compile()
    {
        const uint32_t passCount = static_cast<uint32_t>(m_passes.size());
        if (passCount == 0) return false;

        // ── Build dependency graph ───────────────────────────
        // Dependencies are recorded during setup (ReadTexture, WriteColour+Load, WriteDepth+Load).
        // Deduplicate edges to avoid inflated in-degrees.
        std::vector<std::vector<uint32_t>> adjacency(passCount);
        std::vector<uint32_t> inDegree(passCount, 0);

        for (uint32_t b = 0; b < passCount; ++b)
        {
            auto& deps = m_passes[b].DependsOn;
            std::sort(deps.begin(), deps.end());
            deps.erase(std::unique(deps.begin(), deps.end()), deps.end());

            for (uint32_t a : deps)
            {
                if (a < passCount && a != b)
                {
                    adjacency[a].push_back(b);
                    ++inDegree[b];
                }
            }
        }

        // ── Topological sort (Kahn's algorithm) ──────────────
        std::queue<uint32_t> ready;
        for (uint32_t i = 0; i < passCount; ++i)
        {
            if (inDegree[i] == 0) ready.push(i);
        }

        m_executionOrder.clear();
        m_executionOrder.reserve(passCount);

        while (!ready.empty())
        {
            uint32_t current = ready.front();
            ready.pop();
            m_executionOrder.push_back(current);

            for (uint32_t next : adjacency[current])
            {
                if (--inDegree[next] == 0)
                {
                    ready.push(next);
                }
            }
        }

        if (m_executionOrder.size() != passCount)
        {
            WAYFINDER_ERROR(LogRenderer, "RenderGraph: Cycle detected — {} of {} passes sorted",
                m_executionOrder.size(), passCount);
            return false;
        }

        // ── Dead pass culling ────────────────────────────────
        // A pass is alive if it writes to the swapchain, or if any alive
        // pass reads a resource it produces.
        std::vector<bool> alive(passCount, false);

        // Seed: passes that write to the swapchain are always alive
        for (uint32_t i = 0; i < passCount; ++i)
        {
            if (m_passes[i].SwapchainWrite)
            {
                alive[i] = true;
            }
        }

        // Propagate backwards: if pass B is alive and depends on pass A, then A is alive too.
        bool changed = true;
        while (changed)
        {
            changed = false;
            for (uint32_t i = 0; i < passCount; ++i)
            {
                if (!alive[i]) continue;
                for (uint32_t dep : m_passes[i].DependsOn)
                {
                    if (dep < passCount && !alive[dep])
                    {
                        alive[dep] = true;
                        changed = true;
                    }
                }
            }
        }

        for (uint32_t i = 0; i < passCount; ++i)
        {
            m_passes[i].Culled = !alive[i];
        }

        // Assign sort order
        uint32_t order = 0;
        for (uint32_t idx : m_executionOrder)
        {
            m_passes[idx].SortOrder = order++;
        }

        m_compiled = true;
        return true;
    }

    void RenderGraph::Execute(RenderDevice& device, TransientResourcePool& pool)
    {
        if (!m_compiled) return;

        auto deriveUsage = [](const ResourceEntry& res) -> TextureUsage
        {
            TextureUsage usage = TextureUsage::ColourTarget;
            if (res.Desc.Format == TextureFormat::D32_FLOAT ||
                res.Desc.Format == TextureFormat::D24_UNORM_S8)
            {
                usage = TextureUsage::DepthTarget;
            }
            if (res.IsReadAsSampler) { usage |= TextureUsage::Sampler; }
            return usage;
        };

        // ── Allocate transient textures ──────────────────────
        RenderGraphResources resources;
        resources.m_textures.resize(m_resources.size(), GPUTextureHandle::Invalid());

        for (uint32_t i = 0; i < m_resources.size(); ++i)
        {
            auto& res = m_resources[i];
            if (!res.IsTransient) continue;
            if (res.Desc.Width == 0 || res.Desc.Height == 0) continue;

            TextureCreateDesc texDesc;
            texDesc.width = res.Desc.Width;
            texDesc.height = res.Desc.Height;
            texDesc.format = res.Desc.Format;
            texDesc.usage = deriveUsage(res);

            resources.m_textures[i] = pool.Acquire(texDesc);
        }

        // ── Execute passes in sorted order ───────────────────
        for (uint32_t passIdx : m_executionOrder)
        {
            auto& pass = m_passes[passIdx];
            if (pass.Culled || !pass.Execute) continue;

            if (pass.Type == RenderGraphPassType::Raster)
            {
                // Build the render pass descriptor
                RenderPassDescriptor rpDesc;
                rpDesc.debugName = pass.Name;

                if (pass.SwapchainWrite)
                {
                    rpDesc.targetSwapchain = true;
                    rpDesc.colourAttachment.loadOp = pass.SwapchainWrite->Load;
                    rpDesc.colourAttachment.clearValue = pass.SwapchainWrite->Clear;
                    rpDesc.colourAttachment.storeOp = StoreOp::Store;
                }
                else if (pass.ColourWrite)
                {
                    rpDesc.targetSwapchain = false;
                    rpDesc.colourTarget = resources.GetTexture(pass.ColourWrite->Handle);
                    rpDesc.colourAttachment.loadOp = pass.ColourWrite->Load;
                    rpDesc.colourAttachment.clearValue = pass.ColourWrite->Clear;
                    rpDesc.colourAttachment.storeOp = StoreOp::Store;
                }

                if (pass.DepthWrite)
                {
                    rpDesc.depthAttachment.enabled = true;
                    rpDesc.depthTarget = resources.GetTexture(pass.DepthWrite->Handle);
                    rpDesc.depthAttachment.loadOp = pass.DepthWrite->Load;
                    rpDesc.depthAttachment.clearDepth = pass.DepthWrite->ClearDepth;
                    rpDesc.depthAttachment.storeOp = StoreOp::Store;
                }

                device.BeginRenderPass(rpDesc);
                pass.Execute(device, resources);
                device.EndRenderPass();
            }
            else if (pass.Type == RenderGraphPassType::Compute)
            {
                device.BeginComputePass();
                pass.Execute(device, resources);
                device.EndComputePass();
            }
        }

        // ── Release transient textures back to pool ──────────
        for (uint32_t i = 0; i < m_resources.size(); ++i)
        {
            auto& res = m_resources[i];
            if (!res.IsTransient || !resources.m_textures[i]) continue;

            TextureCreateDesc texDesc;
            texDesc.width = res.Desc.Width;
            texDesc.height = res.Desc.Height;
            texDesc.format = res.Desc.Format;
            texDesc.usage = deriveUsage(res);

            pool.Release(resources.m_textures[i], texDesc);
        }
    }

} // namespace Wayfinder
