#include "rendering/backend/RenderDevice.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/resources/TransientResourcePool.h"

#include <doctest/doctest.h>

#include <string>
#include <vector>

namespace Wayfinder::Tests
{
    // Helper: create a NullDevice + TransientResourcePool for graph execution
    struct GraphTestFixture
    {
        std::unique_ptr<Wayfinder::RenderDevice> m_Device;
        Wayfinder::TransientResourcePool m_Pool;

        GraphTestFixture() : m_Device(Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null))
        {
            m_Pool.Initialise(*m_Device);
        }

        ~GraphTestFixture()
        {
            m_Pool.Shutdown();
        }
    };

    // ── Topological Sort ─────────────────────────────────────

    TEST_CASE("Empty graph does not compile")
    {
        Wayfinder::RenderGraph graph;
        CHECK_FALSE(graph.Compile());
    }

    TEST_CASE("Single pass targeting swapchain compiles")
    {
        Wayfinder::RenderGraph graph;

        graph.AddPass("Only", [](Wayfinder::RenderGraphBuilder& builder)
        {
            builder.SetSwapchainOutput();
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
            };
        });

        CHECK(graph.Compile());
    }

    TEST_CASE("Topological sort respects resource dependencies")
    {
        // Pass A writes SceneColour, Pass B reads SceneColour and writes swapchain.
        // B must execute after A.
        std::vector<std::string> executionOrder;

        Wayfinder::RenderGraph graph;

        Wayfinder::RenderGraphTextureDesc colourDesc;
        colourDesc.Width = 800;
        colourDesc.Height = 600;
        colourDesc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
        colourDesc.DebugName = Wayfinder::WellKnown::SceneColour;

        graph.AddPass("A_WriteColour", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto colour = builder.CreateTransient(colourDesc);
            builder.WriteColour(colour);
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
                executionOrder.push_back("A");
            };
        });

        graph.AddPass("B_ReadAndPresent", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto colour = graph.FindHandle(Wayfinder::WellKnown::SceneColour);
            builder.ReadTexture(colour);
            builder.SetSwapchainOutput();
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
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
        desc.DebugName = "IntermediateColour";

        graph.AddPass("First", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto tex = builder.CreateTransient(desc);
            builder.WriteColour(tex);
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
                executionOrder.push_back("First");
            };
        });

        graph.AddPass("Second", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto tex = graph.FindHandle("IntermediateColour");
            builder.WriteColour(tex, Wayfinder::LoadOp::Load);
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
                executionOrder.push_back("Second");
            };
        });

        graph.AddPass("Present", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto tex = graph.FindHandle("IntermediateColour");
            builder.ReadTexture(tex);
            builder.SetSwapchainOutput();
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
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

        graph.AddPass("Orphan", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto tex = builder.CreateTransient(desc);
            builder.WriteColour(tex);
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
                executionOrder.push_back("Orphan");
            };
        });

        graph.AddPass("Present", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            builder.SetSwapchainOutput();
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
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

        graph.AddPass("Producer", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto tex = builder.CreateTransient(desc);
            builder.WriteColour(tex);
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
                executionOrder.push_back("Producer");
            };
        });

        graph.AddPass("Consumer", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto tex = graph.FindHandle("Needed");
            builder.ReadTexture(tex);
            builder.SetSwapchainOutput();
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
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

        graph.AddPass("PassA", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto a = builder.CreateTransient(descA);
            builder.WriteColour(a);
            auto c = graph.ImportTexture("TexC");
            builder.ReadTexture(c);
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
            };
        });

        graph.AddPass("PassB", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto b = builder.CreateTransient(descB);
            builder.WriteColour(b);
            auto a = graph.FindHandle("TexA");
            builder.ReadTexture(a);
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
            };
        });

        graph.AddPass("PassC", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto c = graph.FindHandle("TexC");
            builder.WriteColour(c, Wayfinder::LoadOp::Clear);
            auto b = graph.FindHandle("TexB");
            builder.ReadTexture(b);
            builder.SetSwapchainOutput();
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
            };
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

        graph.AddComputePass("Compute", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto tex = builder.CreateTransient(desc);
            builder.WriteColour(tex);
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
                executionOrder.push_back("Compute");
            };
        });

        graph.AddPass("Present", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto tex = graph.FindHandle("ComputeOutput");
            builder.ReadTexture(tex);
            builder.SetSwapchainOutput();
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
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

        // NullDevice returns distinguishable handles for textures. This test
        // verifies that the acquire/release cycle works with a null backend.
        auto tex = pool.Acquire(desc);
        CHECK(tex.IsValid());
        pool.Release(tex, desc);

        auto tex2 = pool.Acquire(desc);
        CHECK(tex2.IsValid());

        pool.Shutdown();
    }

    // ── Well-Known Resource Handles ──────────────────────────

    TEST_CASE("Well-known names resolve correctly")
    {
        Wayfinder::RenderGraph graph;

        auto sceneColour = graph.ImportTexture(Wayfinder::WellKnown::SceneColour);
        auto sceneDepth = graph.ImportTexture(Wayfinder::WellKnown::SceneDepth);

        CHECK(sceneColour.IsValid());
        CHECK(sceneDepth.IsValid());
        CHECK_FALSE(sceneColour == sceneDepth);

        // FindHandle returns the same handle
        auto found = graph.FindHandle(Wayfinder::WellKnown::SceneColour);
        CHECK(found == sceneColour);

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

    TEST_CASE("Render graph names remain valid when provided by temporary strings")
    {
        Wayfinder::RenderGraph graph;

        graph.AddPass(std::string{"TemporaryProducer"}, [&](Wayfinder::RenderGraphBuilder& builder)
        {
            Wayfinder::RenderGraphTextureDesc desc;
            desc.Width = 64;
            desc.Height = 64;
            desc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
            desc.DebugName = "TemporaryTexture";

            auto texture = builder.CreateTransient(desc);
            builder.WriteColour(texture);
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
            };
        });

        graph.AddPass(std::string{"TemporaryConsumer"}, [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto texture = graph.FindHandle(std::string{"TemporaryTexture"});
            builder.ReadTexture(texture);
            builder.SetSwapchainOutput();
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
            };
        });

        CHECK(graph.ImportTexture(std::string{"ImportedTexture"}).IsValid());
        CHECK(graph.FindHandle(std::string{"ImportedTexture"}).IsValid());
        CHECK(graph.Compile());
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

        Wayfinder::PipelineCreateDesc pipelineDesc;
        auto pipeline = fixture.m_Device->CreatePipeline(pipelineDesc);
        fixture.m_Device->DestroyPipeline(pipeline);

        Wayfinder::ComputePipelineCreateDesc computePipelineDesc;
        auto computePipeline = fixture.m_Device->CreateComputePipeline(computePipelineDesc);
        fixture.m_Device->DestroyComputePipeline(computePipeline);

        auto sampler = fixture.m_Device->CreateSampler({});
        fixture.m_Device->DestroySampler(sampler);

        const Wayfinder::Extent2D swapchainDimensions = fixture.m_Device->GetSwapchainDimensions();
        CHECK(swapchainDimensions.width == 0);
        CHECK(swapchainDimensions.height == 0);
    }

    // ── Depth Target ─────────────────────────────────────────

    TEST_CASE("Depth target pass compiles and executes")
    {
        std::vector<std::string> executionOrder;
        Wayfinder::RenderGraph graph;

        Wayfinder::RenderGraphTextureDesc colourDesc;
        colourDesc.Width = 800;
        colourDesc.Height = 600;
        colourDesc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
        colourDesc.DebugName = "SceneColour";

        Wayfinder::RenderGraphTextureDesc depthDesc;
        depthDesc.Width = 800;
        depthDesc.Height = 600;
        depthDesc.Format = Wayfinder::TextureFormat::D32_FLOAT;
        depthDesc.DebugName = "SceneDepth";

        graph.AddPass("Scene", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto colour = builder.CreateTransient(colourDesc);
            auto depth = builder.CreateTransient(depthDesc);
            builder.WriteColour(colour);
            builder.WriteDepth(depth);
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
                executionOrder.push_back("Scene");
            };
        });

        graph.AddPass("Composition", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto colour = graph.FindHandle("SceneColour");
            builder.ReadTexture(colour);
            builder.SetSwapchainOutput();
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
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

    // ── Multiple Render Targets ──────────────────────────────

    TEST_CASE("Two-attachment MRT pass compiles and executes")
    {
        // GBuffer pass writes to two colour targets (slots 0 and 1).
        // Composition reads both and writes to swapchain.
        std::vector<std::string> executionOrder;
        Wayfinder::RenderGraph graph;

        Wayfinder::RenderGraphTextureDesc albedoDesc;
        albedoDesc.Width = 800;
        albedoDesc.Height = 600;
        albedoDesc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
        albedoDesc.DebugName = "GBuffer_Albedo";

        Wayfinder::RenderGraphTextureDesc normalDesc;
        normalDesc.Width = 800;
        normalDesc.Height = 600;
        normalDesc.Format = Wayfinder::TextureFormat::RGBA16_FLOAT;
        normalDesc.DebugName = "GBuffer_Normal";

        graph.AddPass("GBuffer", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto albedo = builder.CreateTransient(albedoDesc);
            auto normal = builder.CreateTransient(normalDesc);
            builder.WriteColour(albedo, 0);
            builder.WriteColour(normal, 1);
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
                executionOrder.push_back("GBuffer");
            };
        });

        graph.AddPass("Composition", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto albedo = graph.FindHandle("GBuffer_Albedo");
            auto normal = graph.FindHandle("GBuffer_Normal");
            builder.ReadTexture(albedo);
            builder.ReadTexture(normal);
            builder.SetSwapchainOutput();
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
                executionOrder.push_back("Composition");
            };
        });

        REQUIRE(graph.Compile());

        GraphTestFixture fixture;
        graph.Execute(*fixture.m_Device, fixture.m_Pool);

        REQUIRE(executionOrder.size() == 2);
        CHECK(executionOrder[0] == "GBuffer");
        CHECK(executionOrder[1] == "Composition");
    }

    TEST_CASE("Mixed single and multi-target chain maintains dependency order")
    {
        // A writes slot 0, B writes slots 0+1 (loading slot 0 from A), C reads both → swapchain.
        std::vector<std::string> executionOrder;
        Wayfinder::RenderGraph graph;

        Wayfinder::RenderGraphTextureDesc colourDesc;
        colourDesc.Width = 640;
        colourDesc.Height = 480;
        colourDesc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
        colourDesc.DebugName = "SharedColour";

        Wayfinder::RenderGraphTextureDesc extraDesc;
        extraDesc.Width = 640;
        extraDesc.Height = 480;
        extraDesc.Format = Wayfinder::TextureFormat::RGBA16_FLOAT;
        extraDesc.DebugName = "ExtraTarget";

        graph.AddPass("PassA", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto colour = builder.CreateTransient(colourDesc);
            builder.WriteColour(colour, 0);
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
                executionOrder.push_back("PassA");
            };
        });

        graph.AddPass("PassB", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto colour = graph.FindHandle("SharedColour");
            auto extra = builder.CreateTransient(extraDesc);
            builder.WriteColour(colour, 0, Wayfinder::LoadOp::Load); // depends on PassA
            builder.WriteColour(extra, 1);
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
                executionOrder.push_back("PassB");
            };
        });

        graph.AddPass("PassC", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto colour = graph.FindHandle("SharedColour");
            auto extra = graph.FindHandle("ExtraTarget");
            builder.ReadTexture(colour);
            builder.ReadTexture(extra);
            builder.SetSwapchainOutput();
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
                executionOrder.push_back("PassC");
            };
        });

        REQUIRE(graph.Compile());

        GraphTestFixture fixture;
        graph.Execute(*fixture.m_Device, fixture.m_Pool);

        REQUIRE(executionOrder.size() == 3);
        CHECK(executionOrder[0] == "PassA");
        CHECK(executionOrder[1] == "PassB");
        CHECK(executionOrder[2] == "PassC");
    }

    TEST_CASE("Slot-based WriteColour defaults to slot 0")
    {
        // Verify the convenience overload (no explicit slot) works identically to slot 0.
        std::vector<std::string> executionOrder;
        Wayfinder::RenderGraph graph;

        Wayfinder::RenderGraphTextureDesc desc;
        desc.Width = 800;
        desc.Height = 600;
        desc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
        desc.DebugName = "DefaultSlot";

        graph.AddPass("Writer", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto tex = builder.CreateTransient(desc);
            builder.WriteColour(tex); // no slot = slot 0
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
                executionOrder.push_back("Writer");
            };
        });

        graph.AddPass("Reader", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto tex = graph.FindHandle("DefaultSlot");
            builder.ReadTexture(tex);
            builder.SetSwapchainOutput();
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
                executionOrder.push_back("Reader");
            };
        });

        REQUIRE(graph.Compile());

        GraphTestFixture fixture;
        graph.Execute(*fixture.m_Device, fixture.m_Pool);

        REQUIRE(executionOrder.size() == 2);
        CHECK(executionOrder[0] == "Writer");
        CHECK(executionOrder[1] == "Reader");
    }
}
