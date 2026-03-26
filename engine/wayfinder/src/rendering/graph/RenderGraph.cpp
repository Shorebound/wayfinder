#include "RenderGraph.h"
#include "core/Log.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/graph/RenderPassCapabilities.h"
#include "rendering/resources/TransientResourcePool.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <queue>
#include <string>

namespace Wayfinder
{
    namespace
    {
        template<typename TContainer, typename TIndex>
        decltype(auto) CheckedAt(TContainer& container, TIndex index)
        {
            const auto checkedIndex = static_cast<size_t>(index);
            assert(checkedIndex < container.size());
            return container.at(checkedIndex);
        }

    } // namespace

    // ── Builder ──────────────────────────────────────────────

    RenderGraphBuilder::RenderGraphBuilder(RenderGraph& graph, uint32_t passIndex) : m_graph(graph), m_passIndex(passIndex) {}

    RenderGraphHandle RenderGraphBuilder::CreateTransient(const RenderGraphTextureDesc& desc)
    {
        return m_graph.AllocateResource(desc, InternedString::Intern(desc.DebugName));
    }

    void RenderGraphBuilder::ReadTexture(RenderGraphHandle handle)
    {
        if (!handle.IsValid() || handle.Index >= m_graph.m_resources.size())
        {
            const auto& pass = CheckedAt(m_graph.m_passes, m_passIndex);
            WAYFINDER_ERROR(LogRenderer, "RenderGraph pass '{}': ReadTexture — invalid handle (index={}, valid={})", pass.Name.GetString(), handle.Index, handle.IsValid());
            return;
        }
        auto& pass = CheckedAt(m_graph.m_passes, m_passIndex);
        pass.Reads.push_back(handle);

        auto& res = CheckedAt(m_graph.m_resources, handle.Index);
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
        WriteColour(handle, 0, load, clear);
    }

    void RenderGraphBuilder::WriteColour(RenderGraphHandle handle, uint32_t slot, LoadOp load, ClearValue clear)
    {
        if (!handle.IsValid() || handle.Index >= m_graph.m_resources.size())
        {
            const auto& pass = CheckedAt(m_graph.m_passes, m_passIndex);
            WAYFINDER_ERROR(LogRenderer, "RenderGraph pass '{}': WriteColour — invalid handle (index={}, valid={})", pass.Name.GetString(), handle.Index, handle.IsValid());
            return;
        }
        if (slot >= MAX_COLOUR_TARGETS)
        {
            WAYFINDER_ERROR(LogRenderer, "RenderGraphBuilder::WriteColour: slot {} exceeds MAX_COLOUR_TARGETS ({})", slot, MAX_COLOUR_TARGETS);
            return;
        }

        auto& pass = CheckedAt(m_graph.m_passes, m_passIndex);

        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index, cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        // — slot is bounds-checked against MAX_COLOUR_TARGETS above

        // Reject duplicate slot writes
        if (slot < pass.NumColourWrites && pass.ColourWrites[slot].Handle.IsValid())
        {
            WAYFINDER_ERROR(LogRenderer, "RenderGraphBuilder::WriteColour: duplicate write to slot {} in pass '{}'", slot, pass.Name.GetString());
            return;
        }

        // Reject sparse slots — all slots must be contiguous from 0
        if (slot > pass.NumColourWrites)
        {
            WAYFINDER_ERROR(LogRenderer, "RenderGraphBuilder::WriteColour: slot {} would create a gap (current count: {}) in pass '{}'", slot, pass.NumColourWrites, pass.Name.GetString());
            return;
        }

        // Reject aliasing the same texture into multiple MRT slots
        for (uint32_t i = 0; i < pass.NumColourWrites; ++i)
        {
            if (i != slot && pass.ColourWrites[i].Handle == handle)
            {
                WAYFINDER_ERROR(LogRenderer, "RenderGraphBuilder::WriteColour: texture already bound to slot {} — cannot also bind to slot {} in pass '{}'", i, slot, pass.Name.GetString());
                return;
            }
        }

        pass.ColourWrites[slot] = RenderGraph::ColourWriteInfo{.Handle = handle, .Slot = slot, .Load = load, .Clear = clear};
        // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index, cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        if (slot == pass.NumColourWrites)
        {
            ++pass.NumColourWrites;
        }

        auto& res = CheckedAt(m_graph.m_resources, handle.Index);
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
        if (!handle.IsValid() || handle.Index >= m_graph.m_resources.size())
        {
            const auto& pass = CheckedAt(m_graph.m_passes, m_passIndex);
            WAYFINDER_ERROR(LogRenderer, "RenderGraph pass '{}': WriteDepth — invalid handle (index={}, valid={})", pass.Name.GetString(), handle.Index, handle.IsValid());
            return;
        }
        auto& pass = CheckedAt(m_graph.m_passes, m_passIndex);
        pass.DepthWrite = RenderGraph::DepthWriteInfo{.Handle = handle, .Load = load, .ClearDepth = clearDepth};

        auto& res = CheckedAt(m_graph.m_resources, handle.Index);
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
        auto& pass = CheckedAt(m_graph.m_passes, m_passIndex);
        pass.SwapchainWrite = RenderGraph::SwapchainWriteInfo{.Load = load, .Clear = clear};
    }

    void RenderGraphBuilder::DeclarePassCapabilities(const RenderPassCapabilityMask mask)
    {
        auto& pass = CheckedAt(m_graph.m_passes, m_passIndex);
        pass.DeclaredCapabilities = mask;
    }

    // ── Resources ────────────────────────────────────────────

    GPUTextureHandle RenderGraphResources::GetTexture(RenderGraphHandle handle) const
    {
        if (!handle.IsValid() || handle.Index >= m_textures.size())
        {
            WAYFINDER_VERBOSE(LogRenderer, "RenderGraphResources::GetTexture: invalid handle (index={}, valid={}, count={})", handle.Index, handle.IsValid(), m_textures.size());
            return GPUTextureHandle::Invalid();
        }
        return CheckedAt(m_textures, handle.Index);
    }

    // ── RenderGraph ──────────────────────────────────────────

    RenderGraphHandle RenderGraph::AllocateResource(const RenderGraphTextureDesc& desc, InternedString name)
    {
        RenderGraphHandle handle;
        handle.Index = static_cast<uint32_t>(m_resources.size());

        ResourceEntry entry;
        entry.Desc = desc;
        entry.Name = name;
        m_resources.push_back(entry);

        return handle;
    }

    RenderGraphHandle RenderGraph::ImportTexture(std::string_view name)
    {
        return ImportTexture(InternedString::Intern(name));
    }

    RenderGraphHandle RenderGraph::ImportTexture(const InternedString& name)
    {
        for (uint32_t i = 0; i < m_resources.size(); ++i)
        {
            if (CheckedAt(m_resources, i).Name == name)
            {
                return {i};
            }
        }

        const RenderGraphTextureDesc desc;
        return AllocateResource(desc, name);
    }

    RenderGraphHandle RenderGraph::ImportTexture(const GraphTextureId id)
    {
        return ImportTexture(GraphTextureIntern(id));
    }

    RenderGraphHandle RenderGraph::FindHandle(std::string_view name) const
    {
        const auto internedName = InternedString::Intern(name);

        for (uint32_t i = 0; i < m_resources.size(); ++i)
        {
            if (CheckedAt(m_resources, i).Name == internedName)
            {
                return {i};
            }
        }
        return {};
    }

    RenderGraphHandle RenderGraph::FindHandle(const GraphTextureId id) const
    {
        const InternedString& name = GraphTextureIntern(id);
        for (uint32_t i = 0; i < m_resources.size(); ++i)
        {
            if (CheckedAt(m_resources, i).Name == name)
            {
                return {i};
            }
        }
        return {};
    }

    RenderGraphHandle RenderGraph::FindHandleChecked(std::string_view name) const
    {
        RenderGraphHandle h = FindHandle(name);
        if (!h.IsValid())
        {
            WAYFINDER_ERROR(LogRenderer, "RenderGraph: FindHandleChecked missing resource '{}'", name);
            assert(false && "RenderGraph: FindHandleChecked missing resource");
        }
        return h;
    }

    RenderGraphHandle RenderGraph::FindHandleChecked(const GraphTextureId id) const
    {
        RenderGraphHandle h = FindHandle(id);
        if (!h.IsValid())
        {
            WAYFINDER_ERROR(LogRenderer, "RenderGraph: FindHandleChecked missing resource '{}'", GraphTextureIntern(id).GetString());
            assert(false && "RenderGraph: FindHandleChecked missing resource");
        }
        return h;
    }

    bool RenderGraph::Compile()
    {
        const auto passCount = static_cast<uint32_t>(m_passes.size());
        if (passCount == 0)
        {
            return false;
        }

        // ── Build dependency graph ───────────────────────────
        // Dependencies are recorded during setup (ReadTexture, WriteColour+Load, WriteDepth+Load).
        // Deduplicate edges to avoid inflated in-degrees.
        std::vector<std::vector<uint32_t>> adjacency(passCount);
        std::vector<uint32_t> inDegree(passCount, 0);

        for (uint32_t b = 0; b < passCount; ++b)
        {
            auto& deps = CheckedAt(m_passes, b).DependsOn;
            std::ranges::sort(deps);
            deps.erase(std::ranges::unique(deps).begin(), deps.end());

            for (const uint32_t a : deps)
            {
                if (a < passCount && a != b)
                {
                    CheckedAt(adjacency, a).push_back(b);
                    ++CheckedAt(inDegree, b);
                }
            }
        }

        // ── Topological sort (Kahn's algorithm) ──────────────
        std::queue<uint32_t> ready;
        for (uint32_t i = 0; i < passCount; ++i)
        {
            if (CheckedAt(inDegree, i) == 0)
            {
                ready.push(i);
            }
        }

        m_executionOrder.clear();
        m_executionOrder.reserve(passCount);

        while (!ready.empty())
        {
            const uint32_t current = ready.front();
            ready.pop();
            m_executionOrder.push_back(current);

            for (const uint32_t next : CheckedAt(adjacency, current))
            {
                if (--CheckedAt(inDegree, next) == 0)
                {
                    ready.push(next);
                }
            }
        }

        if (m_executionOrder.size() != passCount)
        {
            WAYFINDER_ERROR(LogRenderer, "RenderGraph: Cycle detected — {} of {} passes sorted", m_executionOrder.size(), passCount);
            return false;
        }

        // ── Dead pass culling ────────────────────────────────
        // A pass is alive if it writes to the swapchain, or if any alive
        // pass reads a resource it produces.
        std::vector<bool> alive(passCount, false);

        // Seed: passes that write to the swapchain are always alive
        for (uint32_t i = 0; i < passCount; ++i)
        {
            if (CheckedAt(m_passes, i).SwapchainWrite)
            {
                CheckedAt(alive, i) = true;
            }
        }

        // Propagate backwards: if pass B is alive and depends on pass A, then A is alive too.
        bool changed = true;
        while (changed)
        {
            changed = false;
            for (uint32_t i = 0; i < passCount; ++i)
            {
                if (!CheckedAt(alive, i))
                {
                    continue;
                }
                for (const uint32_t dep : CheckedAt(m_passes, i).DependsOn)
                {
                    if (dep < passCount && !CheckedAt(alive, dep))
                    {
                        CheckedAt(alive, dep) = true;
                        changed = true;
                    }
                }
            }
        }

        for (uint32_t i = 0; i < passCount; ++i)
        {
            CheckedAt(m_passes, i).Culled = !CheckedAt(alive, i);
        }

#if !defined(NDEBUG)
        for (uint32_t i = 0; i < passCount; ++i)
        {
            auto& pass = CheckedAt(m_passes, i);
            if (pass.Culled || !pass.DeclaredCapabilities.has_value())
            {
                continue;
            }
            const RenderPassCapabilityMask caps = *pass.DeclaredCapabilities;
            if ((caps & RenderPassCapabilities::RASTER_SCENE_GEOMETRY) != 0)
            {
                if (pass.NumColourWrites == 0 && !pass.DepthWrite.has_value())
                {
                    WAYFINDER_WARN(LogRenderer, "RenderGraph: pass '{}' declared RASTER_SCENE_GEOMETRY but has no colour or depth attachment", pass.Name.GetString());
                }
            }
            if ((caps & RenderPassCapabilities::RASTER_OVERLAY_OR_DEBUG) != 0)
            {
                if (pass.NumColourWrites == 0)
                {
                    WAYFINDER_WARN(LogRenderer, "RenderGraph: pass '{}' declared RASTER_OVERLAY_OR_DEBUG but has no colour attachment", pass.Name.GetString());
                }
            }
            if ((caps & RenderPassCapabilities::FULLSCREEN_COMPOSITE) != 0)
            {
                if (!pass.SwapchainWrite.has_value())
                {
                    WAYFINDER_WARN(LogRenderer, "RenderGraph: pass '{}' declared FULLSCREEN_COMPOSITE but does not set swapchain output", pass.Name.GetString());
                }
            }
        }
#endif

        // Assign sort order
        uint32_t order = 0;
        for (const uint32_t idx : m_executionOrder)
        {
            CheckedAt(m_passes, idx).SortOrder = order++;
        }

#if !defined(NDEBUG)
        {
            static uint32_t sOrderLogThrottle = 0;
            if ((sOrderLogThrottle++ % 120u) == 0u)
            {
                std::string orderLine;
                for (const uint32_t idx : m_executionOrder)
                {
                    const auto& p = CheckedAt(m_passes, idx);
                    if (!orderLine.empty())
                    {
                        orderLine += " -> ";
                    }
                    orderLine += p.Name.GetString();
                    if (p.Culled)
                    {
                        orderLine += " (culled)";
                    }
                }
                WAYFINDER_VERBOSE(LogRenderer, "RenderGraph compile order: {}", orderLine);
            }
        }
#endif

        m_compiled = true;
        return true;
    }

    void RenderGraph::Execute(RenderDevice& device, TransientResourcePool& pool)
    {
        if (!m_compiled)
        {
            return;
        }

        auto deriveUsage = [](const ResourceEntry& res) -> TextureUsage
        {
            TextureUsage usage = TextureUsage::ColourTarget;
            if (res.Desc.Format == TextureFormat::D32_FLOAT || res.Desc.Format == TextureFormat::D24_UNORM_S8)
            {
                usage = TextureUsage::DepthTarget;
            }
            if (res.IsReadAsSampler)
            {
                usage |= TextureUsage::Sampler;
            }
            return usage;
        };

        // ── Allocate transient textures ──────────────────────
        RenderGraphResources resources;
        resources.m_textures.resize(m_resources.size(), GPUTextureHandle::Invalid());

        for (uint32_t i = 0; i < m_resources.size(); ++i)
        {
            auto& res = CheckedAt(m_resources, i);
            if (!res.IsTransient)
            {
                continue;
            }
            if (res.Desc.Width == 0 || res.Desc.Height == 0)
            {
                continue;
            }

            TextureCreateDesc texDesc;
            texDesc.width = res.Desc.Width;
            texDesc.height = res.Desc.Height;
            texDesc.format = res.Desc.Format;
            texDesc.usage = deriveUsage(res);

            CheckedAt(resources.m_textures, i) = pool.Acquire(texDesc);
        }

        // ── Execute passes in sorted order ───────────────────
        for (const uint32_t passIdx : m_executionOrder)
        {
            auto& pass = CheckedAt(m_passes, passIdx);
            if (pass.Culled || !pass.Execute)
            {
                continue;
            }

            if (pass.Type == RenderGraphPassType::Raster)
            {
                const GPUDebugScope passDebugScope(device, pass.Name.GetString());

                // Build the render pass descriptor
                RenderPassDescriptor rpDesc;
                rpDesc.debugName = pass.Name.GetString();

                if (pass.SwapchainWrite && pass.NumColourWrites > 0)
                {
                    WAYFINDER_ERROR(LogRenderer, "RenderGraph: pass '{}' has both SwapchainWrite and {} ColourWrites — these are mutually exclusive, skipping pass", pass.Name.GetString(), pass.NumColourWrites);
                    continue;
                }

                // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index, cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
                if (pass.SwapchainWrite)
                {
                    rpDesc.targetSwapchain = true;
                    rpDesc.numColourTargets = 1;
                    rpDesc.colourAttachments[0].loadOp = pass.SwapchainWrite->Load;
                    rpDesc.colourAttachments[0].clearValue = pass.SwapchainWrite->Clear;
                    rpDesc.colourAttachments[0].storeOp = StoreOp::Store;
                }
                else if (pass.NumColourWrites > 0)
                {
                    rpDesc.targetSwapchain = false;
                    rpDesc.numColourTargets = pass.NumColourWrites;

                    for (uint32_t i = 0; i < pass.NumColourWrites; ++i)
                    {
                        const auto& cw = pass.ColourWrites[i];
                        rpDesc.colourAttachments[i].target = resources.GetTexture(cw.Handle);
                        rpDesc.colourAttachments[i].loadOp = cw.Load;
                        rpDesc.colourAttachments[i].clearValue = cw.Clear;
                        rpDesc.colourAttachments[i].storeOp = StoreOp::Store;
                    }
                }
                else
                {
                    rpDesc.targetSwapchain = false;
                    rpDesc.numColourTargets = 0;
                }
                // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index, cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

                if (pass.DepthWrite)
                {
                    rpDesc.depthAttachment.enabled = true;
                    rpDesc.depthTarget = resources.GetTexture(pass.DepthWrite->Handle);
                    rpDesc.depthAttachment.loadOp = pass.DepthWrite->Load;
                    rpDesc.depthAttachment.clearDepth = pass.DepthWrite->ClearDepth;
                    rpDesc.depthAttachment.storeOp = StoreOp::Store;
                }

                if (device.BeginRenderPass(rpDesc))
                {
                    pass.Execute(device, resources);
                    device.EndRenderPass();
                }
            }
            else if (pass.Type == RenderGraphPassType::Compute)
            {
                const GPUDebugScope passDebugScope(device, pass.Name.GetString());
                device.BeginComputePass();
                pass.Execute(device, resources);
                device.EndComputePass();
            }
        }

        // ── Release transient textures back to pool ──────────
        for (uint32_t i = 0; i < m_resources.size(); ++i)
        {
            auto& res = CheckedAt(m_resources, i);
            if (!res.IsTransient || !CheckedAt(resources.m_textures, i))
            {
                continue;
            }

            TextureCreateDesc texDesc;
            texDesc.width = res.Desc.Width;
            texDesc.height = res.Desc.Height;
            texDesc.format = res.Desc.Format;
            texDesc.usage = deriveUsage(res);

            pool.Release(CheckedAt(resources.m_textures, i), texDesc);
        }
    }

} // namespace Wayfinder
