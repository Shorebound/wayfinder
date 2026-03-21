#include "rendering/RenderGraph.h"
#include "rendering/RenderDevice.h"
#include "rendering/TransientResourcePool.h"

#include <doctest/doctest.h>

#include <string>
#include <vector>

namespace
{
    // Helper: create a NullDevice + TransientResourcePool for graph execution
    struct GraphTestFixture
    {
        std::unique_ptr<Wayfinder::RenderDevice> m_Device;
        Wayfinder::TransientResourcePool m_Pool;

        GraphTestFixture()
            : m_Device(Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null))
        {
            m_Pool.Initialise(*m_Device);
        }

        ~GraphTestFixture() { m_Pool.Shutdown(); }
    };
}

// ── Topological Sort ─────────────────────────────────────

TEST_CASE("Empty graph does not compile")
{
    Wayfinder::RenderGraph graph;
    CHECK_FALSE(graph.Compile());
}

TEST_CASE("Single pass targeting swapchain compiles")
{
    Wayfinder::RenderGraph graph;

    graph.AddPass("Only", [](Wayfinder::RenderGraphBuilder& builder) -> Wayfinder::RenderGraphExecuteFn {
        builder.SetSwapchainOutput();
        return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&) {};
    });

    CHECK(graph.Compile());
}

TEST_CASE("Topological sort respects resource dependencies")
{
    // Pass A writes SceneColor, Pass B reads SceneColor and writes swapchain.
    // B must execute after A.
    std::vector<std::string> executionOrder;

    Wayfinder::RenderGraph graph;

    Wayfinder::RenderGraphTextureDesc colorDesc;
    colorDesc.Width = 800;
    colorDesc.Height = 600;
    colorDesc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
    colorDesc.DebugName = Wayfinder::WellKnown::SceneColor;

    graph.AddPass("A_WriteColor", [&](Wayfinder::RenderGraphBuilder& builder) -> Wayfinder::RenderGraphExecuteFn {
        auto colour = builder.CreateTransient(colorDesc);
        builder.WriteColor(colour);
        return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&) {
            executionOrder.push_back("A");
        };
    });

    graph.AddPass("B_ReadAndPresent", [&](Wayfinder::RenderGraphBuilder& builder) -> Wayfinder::RenderGraphExecuteFn {
        auto colour = graph.FindHandle(Wayfinder::WellKnown::SceneColor);
        builder.ReadTexture(colour);
        builder.SetSwapchainOutput();
        return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&) {
            executionOrder.push_back("B");
        };
    });

    REQUIRE(graph.Compile());

    GraphTestFixture fixture;
    graph.Execute(*fixture.m_Device, fixture.m_Pool);

    REQUIRE(executionOrder.size() == 2);
    CHECK(executionOrder[0] == "A");
    CHECK(executionOrder[1] == "B");
}

TEST_CASE("Three-pass chain executes in dependency order")
{
    // A writes colour -> B reads colour, writes to colour (load) -> C reads colour, writes swapchain
    std::vector<std::string> executionOrder;
    Wayfinder::RenderGraph graph;

    Wayfinder::RenderGraphTextureDesc desc;
    desc.Width = 640;
    desc.Height = 480;
    desc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
    desc.DebugName = "IntermediateColor";

    graph.AddPass("First", [&](Wayfinder::RenderGraphBuilder& builder) -> Wayfinder::RenderGraphExecuteFn {
        auto tex = builder.CreateTransient(desc);
        builder.WriteColor(tex);
        return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&) {
            executionOrder.push_back("First");
        };
    });

    graph.AddPass("Second", [&](Wayfinder::RenderGraphBuilder& builder) -> Wayfinder::RenderGraphExecuteFn {
        auto tex = graph.FindHandle("IntermediateColor");
        builder.WriteColor(tex, Wayfinder::LoadOp::Load);
        return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&) {
            executionOrder.push_back("Second");
        };
    });

    graph.AddPass("Present", [&](Wayfinder::RenderGraphBuilder& builder) -> Wayfinder::RenderGraphExecuteFn {
        auto tex = graph.FindHandle("IntermediateColor");
        builder.ReadTexture(tex);
        builder.SetSwapchainOutput();
        return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&) {
            executionOrder.push_back("Present");
        };
    });

    REQUIRE(graph.Compile());

    GraphTestFixture fixture;
    graph.Execute(*fixture.m_Device, fixture.m_Pool);

    REQUIRE(executionOrder.size() == 3);
    CHECK(executionOrder[0] == "First");
    CHECK(executionOrder[1] == "Second");
    CHECK(executionOrder[2] == "Present");
}

// ── Pass Culling ─────────────────────────────────────────

TEST_CASE("Orphan pass is culled")
{
    // Pass A writes to a texture that nobody reads and does not target swapchain.
    // Pass B writes to swapchain. Only B should execute.
    std::vector<std::string> executionOrder;
    Wayfinder::RenderGraph graph;

    Wayfinder::RenderGraphTextureDesc desc;
    desc.Width = 800;
    desc.Height = 600;
    desc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
    desc.DebugName = "Orphan";

    graph.AddPass("Orphan", [&](Wayfinder::RenderGraphBuilder& builder) -> Wayfinder::RenderGraphExecuteFn {
        auto tex = builder.CreateTransient(desc);
        builder.WriteColor(tex);
        return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&) {
            executionOrder.push_back("Orphan");
        };
    });

    graph.AddPass("Present", [&](Wayfinder::RenderGraphBuilder& builder) -> Wayfinder::RenderGraphExecuteFn {
        builder.SetSwapchainOutput();
        return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&) {
            executionOrder.push_back("Present");
        };
    });

    REQUIRE(graph.Compile());

    GraphTestFixture fixture;
    graph.Execute(*fixture.m_Device, fixture.m_Pool);

    REQUIRE(executionOrder.size() == 1);
    CHECK(executionOrder[0] == "Present");
}

TEST_CASE("Transitive dependency keeps producer alive")
{
    // A writes texture -> B reads texture, writes swapchain
    // A must be kept alive because B depends on it.
    std::vector<std::string> executionOrder;
    Wayfinder::RenderGraph graph;

    Wayfinder::RenderGraphTextureDesc desc;
    desc.Width = 800;
    desc.Height = 600;
    desc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
    desc.DebugName = "Needed";

    graph.AddPass("Producer", [&](Wayfinder::RenderGraphBuilder& builder) -> Wayfinder::RenderGraphExecuteFn {
        auto tex = builder.CreateTransient(desc);
        builder.WriteColor(tex);
        return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&) {
            executionOrder.push_back("Producer");
        };
    });

    graph.AddPass("Consumer", [&](Wayfinder::RenderGraphBuilder& builder) -> Wayfinder::RenderGraphExecuteFn {
        auto tex = graph.FindHandle("Needed");
        builder.ReadTexture(tex);
        builder.SetSwapchainOutput();
        return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&) {
            executionOrder.push_back("Consumer");
        };
    });

    REQUIRE(graph.Compile());

    GraphTestFixture fixture;
    graph.Execute(*fixture.m_Device, fixture.m_Pool);

    REQUIRE(executionOrder.size() == 2);
    CHECK(executionOrder[0] == "Producer");
    CHECK(executionOrder[1] == "Consumer");
}

// ── Cycle Detection ──────────────────────────────────────

TEST_CASE("Complex dependency pattern compiles without cycle")
{
    // The graph API prevents cycles by construction: WrittenByPass is recorded
    // during AddPass setup, so a pass can only depend on writers already set up.
    // This three-pass pattern would be cyclic if dependencies could form
    // retroactively, but compiles successfully due to sequential setup semantics.

    Wayfinder::RenderGraph graph;

    Wayfinder::RenderGraphTextureDesc descA, descB, descC;
    descA.Width = descB.Width = descC.Width = 100;
    descA.Height = descB.Height = descC.Height = 100;
    descA.Format = descB.Format = descC.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
    descA.DebugName = "TexA";
    descB.DebugName = "TexB";
    descC.DebugName = "TexC";

    graph.AddPass("PassA", [&](Wayfinder::RenderGraphBuilder& builder) -> Wayfinder::RenderGraphExecuteFn {
        auto a = builder.CreateTransient(descA);
        builder.WriteColor(a);
        auto c = graph.ImportTexture("TexC");
        builder.ReadTexture(c);
        return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&) {};
    });

    graph.AddPass("PassB", [&](Wayfinder::RenderGraphBuilder& builder) -> Wayfinder::RenderGraphExecuteFn {
        auto b = builder.CreateTransient(descB);
        builder.WriteColor(b);
        auto a = graph.FindHandle("TexA");
        builder.ReadTexture(a);
        return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&) {};
    });

    graph.AddPass("PassC", [&](Wayfinder::RenderGraphBuilder& builder) -> Wayfinder::RenderGraphExecuteFn {
        auto c = graph.FindHandle("TexC");
        builder.WriteColor(c, Wayfinder::LoadOp::Clear);
        auto b = graph.FindHandle("TexB");
        builder.ReadTexture(b);
        builder.SetSwapchainOutput();
        return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&) {};
    });

    CHECK(graph.Compile());
}

// ── Compute Pass ─────────────────────────────────────────

TEST_CASE("Compute pass integrates into the graph")
{
    std::vector<std::string> executionOrder;
    Wayfinder::RenderGraph graph;

    Wayfinder::RenderGraphTextureDesc desc;
    desc.Width = 256;
    desc.Height = 256;
    desc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
    desc.DebugName = "ComputeOutput";

    graph.AddComputePass("Compute", [&](Wayfinder::RenderGraphBuilder& builder) -> Wayfinder::RenderGraphExecuteFn {
        auto tex = builder.CreateTransient(desc);
        builder.WriteColor(tex);
        return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&) {
            executionOrder.push_back("Compute");
        };
    });

    graph.AddPass("Present", [&](Wayfinder::RenderGraphBuilder& builder) -> Wayfinder::RenderGraphExecuteFn {
        auto tex = graph.FindHandle("ComputeOutput");
        builder.ReadTexture(tex);
        builder.SetSwapchainOutput();
        return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&) {
            executionOrder.push_back("Present");
        };
    });

    REQUIRE(graph.Compile());

    GraphTestFixture fixture;
    graph.Execute(*fixture.m_Device, fixture.m_Pool);

    REQUIRE(executionOrder.size() == 2);
    CHECK(executionOrder[0] == "Compute");
    CHECK(executionOrder[1] == "Present");
}

// ── Transient Resource Allocation ────────────────────────

TEST_CASE("Transient pool acquire and release roundtrip")
{
    auto device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null);
    Wayfinder::TransientResourcePool pool;
    pool.Initialise(*device);

    Wayfinder::TextureCreateDesc desc;
    desc.width = 800;
    desc.height = 600;
    desc.format = Wayfinder::TextureFormat::RGBA8_UNORM;
    desc.usage = Wayfinder::TextureUsage::ColourTarget;

    // NullDevice returns invalid handles for textures. This test verifies that
    // the acquire/release cycle doesn't crash with a null backend.  The reuse
    // assertion is vacuous here because Release is a no-op for invalid handles.
    // A real reuse test requires a device that produces valid texture handles.
    auto tex = pool.Acquire(desc);
    CHECK_FALSE(tex.IsValid()); // NullDevice yields invalid handles
    pool.Release(tex, desc);

    auto tex2 = pool.Acquire(desc);
    CHECK_FALSE(tex2.IsValid());

    pool.Shutdown();
}

// ── Well-Known Resource Handles ──────────────────────────

TEST_CASE("Well-known names resolve correctly")
{
    Wayfinder::RenderGraph graph;

    auto sceneColor = graph.ImportTexture(Wayfinder::WellKnown::SceneColor);
    auto sceneDepth = graph.ImportTexture(Wayfinder::WellKnown::SceneDepth);

    CHECK(sceneColor.IsValid());
    CHECK(sceneDepth.IsValid());
    CHECK_FALSE(sceneColor == sceneDepth);

    // FindHandle returns the same handle
    auto found = graph.FindHandle(Wayfinder::WellKnown::SceneColor);
    CHECK(found == sceneColor);

    // Unknown name returns invalid handle
    auto unknown = graph.FindHandle("NonExistent");
    CHECK_FALSE(unknown.IsValid());
}

TEST_CASE("Duplicate import returns same handle")
{
    Wayfinder::RenderGraph graph;

    auto first = graph.ImportTexture("MyTexture");
    auto second = graph.ImportTexture("MyTexture");

    CHECK(first == second);
}

// ── Device Factory ───────────────────────────────────────

TEST_CASE("NullDevice supports all render graph operations")
{
    GraphTestFixture fixture;

    // Verify NullDevice can handle all graph-related operations
    Wayfinder::RenderPassDescriptor rpDesc;
    rpDesc.debugName = "TestPass";
    rpDesc.targetSwapchain = false;

    // These should all be no-ops without crashing
    fixture.m_Device->BeginRenderPass(rpDesc);
    fixture.m_Device->EndRenderPass();

    fixture.m_Device->BeginComputePass();
    fixture.m_Device->EndComputePass();

    auto tex = fixture.m_Device->CreateTexture({});
    fixture.m_Device->DestroyTexture(tex);

    auto buf = fixture.m_Device->CreateBuffer({});
    fixture.m_Device->DestroyBuffer(buf);

    auto shader = fixture.m_Device->CreateShader({});
    fixture.m_Device->DestroyShader(shader);

    auto pipeline = fixture.m_Device->CreatePipeline({});
    fixture.m_Device->DestroyPipeline(pipeline);

    auto computePipeline = fixture.m_Device->CreateComputePipeline({});
    fixture.m_Device->DestroyComputePipeline(computePipeline);

    auto sampler = fixture.m_Device->CreateSampler({});
    fixture.m_Device->DestroySampler(sampler);

    uint32_t w, h;
    fixture.m_Device->GetSwapchainDimensions(w, h);
    CHECK(w == 0);
    CHECK(h == 0);
}

// ── Depth Target ─────────────────────────────────────────

TEST_CASE("Depth target pass compiles and executes")
{
    std::vector<std::string> executionOrder;
    Wayfinder::RenderGraph graph;

    Wayfinder::RenderGraphTextureDesc colorDesc;
    colorDesc.Width = 800;
    colorDesc.Height = 600;
    colorDesc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
    colorDesc.DebugName = "SceneColor";

    Wayfinder::RenderGraphTextureDesc depthDesc;
    depthDesc.Width = 800;
    depthDesc.Height = 600;
    depthDesc.Format = Wayfinder::TextureFormat::D32_FLOAT;
    depthDesc.DebugName = "SceneDepth";

    graph.AddPass("Scene", [&](Wayfinder::RenderGraphBuilder& builder) -> Wayfinder::RenderGraphExecuteFn {
        auto colour = builder.CreateTransient(colorDesc);
        auto depth = builder.CreateTransient(depthDesc);
        builder.WriteColor(colour);
        builder.WriteDepth(depth);
        return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&) {
            executionOrder.push_back("Scene");
        };
    });

    graph.AddPass("Composition", [&](Wayfinder::RenderGraphBuilder& builder) -> Wayfinder::RenderGraphExecuteFn {
        auto colour = graph.FindHandle("SceneColor");
        builder.ReadTexture(colour);
        builder.SetSwapchainOutput();
        return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&) {
            executionOrder.push_back("Composition");
        };
    });

    REQUIRE(graph.Compile());

    GraphTestFixture fixture;
    graph.Execute(*fixture.m_Device, fixture.m_Pool);

    REQUIRE(executionOrder.size() == 2);
    CHECK(executionOrder[0] == "Scene");
    CHECK(executionOrder[1] == "Composition");
}
