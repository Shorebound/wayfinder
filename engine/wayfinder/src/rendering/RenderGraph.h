#pragma once

#include "RenderTypes.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace Wayfinder
{
    class RenderDevice;
    class TransientResourcePool;

    // ── Resource Handle ──────────────────────────────────────

    struct RenderGraphHandle
    {
        uint32_t Index = UINT32_MAX;
        bool IsValid() const { return Index != UINT32_MAX; }
        bool operator==(const RenderGraphHandle&) const = default;
    };

    // ── Well-Known Resource Names ────────────────────────────
    // Published by the engine so features can reference them by name.

    namespace WellKnown
    {
        inline constexpr const char* SceneColor = "SceneColor";
        inline constexpr const char* SceneDepth = "SceneDepth";
    }

    // ── Texture Description for Graph Resources ──────────────

    struct RenderGraphTextureDesc
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        TextureFormat Format = TextureFormat::RGBA8_UNORM;
        const char* DebugName = "";
    };

    // ── Forward Declarations ─────────────────────────────────

    class RenderGraph;
    class RenderGraphResources;

    // ── Builder ──────────────────────────────────────────────
    // Passed to each pass's setup function. Passes declare resource
    // dependencies (reads and writes) through this interface.

    class RenderGraphBuilder
    {
    public:
        // Create a new transient texture owned by the graph.
        RenderGraphHandle CreateTransient(const RenderGraphTextureDesc& desc);

        // Declare this pass reads a texture (for sampling in a shader).
        void ReadTexture(RenderGraphHandle handle);

        // Declare this pass writes to a color render target.
        void WriteColor(RenderGraphHandle handle, LoadOp load = LoadOp::Clear, ClearValue clear = {});

        // Declare this pass writes to a depth render target.
        void WriteDepth(RenderGraphHandle handle, LoadOp load = LoadOp::Clear, float clearDepth = 1.0f);

        // This pass writes directly to the swapchain.
        void SetSwapchainOutput(LoadOp load = LoadOp::Clear, ClearValue clear = {});

    private:
        friend class RenderGraph;
        RenderGraphBuilder(RenderGraph& graph, uint32_t passIndex);

        RenderGraph& m_graph;
        uint32_t m_passIndex;
    };

    // ── Resolved Resources ───────────────────────────────────
    // Provides access to GPU textures during pass execution.

    class RenderGraphResources
    {
    public:
        GPUTextureHandle GetTexture(RenderGraphHandle handle) const;

    private:
        friend class RenderGraph;
        std::vector<GPUTextureHandle> m_textures;
    };

    // ── Pass Types ───────────────────────────────────────────

    using RenderGraphExecuteFn = std::function<void(RenderDevice& device, const RenderGraphResources& resources)>;

    enum class RenderGraphPassType
    {
        Raster,
        Compute,
    };

    // ── Render Graph ─────────────────────────────────────────
    // Built each frame, compiled once, executed once, then discarded.
    // Passes declare resource dependencies, the graph resolves execution
    // order via topological sort, and allocates transient textures.

    using PassSetupFn = std::function<RenderGraphExecuteFn(RenderGraphBuilder& builder)>;

    class RenderGraph
    {
    public:
        RenderGraph() = default;

        // Add a raster pass. The setup function declares dependencies
        // and returns the execute callback.
        void AddPass(const std::string& name, PassSetupFn setup);

        // Add a compute pass (same interface — dependencies determine ordering).
        void AddComputePass(const std::string& name, PassSetupFn setup);

        // Import a named resource handle so passes can reference it by name.
        RenderGraphHandle ImportTexture(const std::string& name);

        // Look up a previously imported or created named resource.
        RenderGraphHandle FindHandle(const std::string& name) const;

        // Compile: topological sort on dependencies + dead pass culling.
        bool Compile();

        // Execute all live passes in sorted order.
        void Execute(RenderDevice& device, TransientResourcePool& pool);

    private:
        friend class RenderGraphBuilder;

        struct ResourceEntry
        {
            RenderGraphTextureDesc Desc;
            std::string Name;
            bool IsTransient = true;
            bool IsReadAsSampler = false;
            uint32_t WrittenByPass = UINT32_MAX;  // Last pass that writes this
            uint32_t LastReadByPass = UINT32_MAX;
        };

        struct ColorWriteInfo
        {
            RenderGraphHandle Handle;
            LoadOp Load = LoadOp::Clear;
            ClearValue Clear{};
        };

        struct DepthWriteInfo
        {
            RenderGraphHandle Handle;
            LoadOp Load = LoadOp::Clear;
            float ClearDepth = 1.0f;
        };

        struct SwapchainWriteInfo
        {
            LoadOp Load = LoadOp::Clear;
            ClearValue Clear{};
        };

        struct PassEntry
        {
            std::string Name;
            RenderGraphPassType Type = RenderGraphPassType::Raster;
            RenderGraphExecuteFn Execute;

            std::vector<RenderGraphHandle> Reads;
            std::vector<uint32_t> DependsOn; // Direct pass indices this pass depends on

            std::optional<ColorWriteInfo> ColorWrite;
            std::optional<DepthWriteInfo> DepthWrite;
            std::optional<SwapchainWriteInfo> SwapchainWrite;

            bool Culled = false;
            uint32_t SortOrder = UINT32_MAX;
        };

        RenderGraphHandle AllocateResource(const RenderGraphTextureDesc& desc, const std::string& name = "");

        std::vector<ResourceEntry> m_resources;
        std::vector<PassEntry> m_passes;
        std::vector<uint32_t> m_executionOrder;
        bool m_compiled = false;
    };

} // namespace Wayfinder
