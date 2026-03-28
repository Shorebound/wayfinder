#include "rendering/backend/RenderDevice.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/materials/RenderingEffects.h"
#include "rendering/pipeline/BuiltInUBOs.h"
#include "rendering/resources/TransientResourcePool.h"

#include <doctest/doctest.h>

#include <string>
#include <vector>

// Doctest CHECK with operator[] — file-wide suppression for test code.
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access, modernize-use-emplace, modernize-use-nodiscard)

namespace Wayfinder::Tests
{
    namespace
    {
        class TrackingRenderDevice final : public Wayfinder::RenderDevice
        {
        public:
            Result<void> Initialise(Wayfinder::Window&) override
            {
                return {};
            }

            void Shutdown() override {}

            bool BeginFrame() override
            {
                m_events.emplace_back("BeginFrame");
                return true;
            }

            void EndFrame() override
            {
                m_events.emplace_back("EndFrame");
            }

            void PushDebugGroup(std::string_view name) override
            {
                m_events.emplace_back(std::string("PushDebugGroup:") + std::string(name));
            }

            void PopDebugGroup() override
            {
                m_events.emplace_back("PopDebugGroup");
            }

            bool BeginRenderPass(const Wayfinder::RenderPassDescriptor& descriptor) override
            {
                m_events.emplace_back(std::string("BeginRenderPass:") + std::string(descriptor.debugName));
                m_capturedDescriptors.push_back(descriptor);
                // Own the debug name string so it doesn't dangle
                m_capturedNames.push_back(std::string(descriptor.debugName));
                m_capturedDescriptors.back().debugName = m_capturedNames.back();
                return true;
            }

            void EndRenderPass() override
            {
                m_events.emplace_back("EndRenderPass");
            }

            GPUShaderHandle CreateShader(const ShaderCreateDesc&) override
            {
                return {};
            }

            void DestroyShader(GPUShaderHandle) override {}

            GPUPipelineHandle CreatePipeline(const PipelineCreateDesc&) override
            {
                return {};
            }

            void DestroyPipeline(GPUPipelineHandle) override {}
            void BindPipeline(GPUPipelineHandle) override {}

            GPUBufferHandle CreateBuffer(const BufferCreateDesc&) override
            {
                return GPUBufferHandle{.Index = m_nextId++, .Generation = 1};
            }

            void DestroyBuffer(GPUBufferHandle) override {}
            void UploadToBuffer(GPUBufferHandle, const void*, BufferUploadRegion) override {}
            void BindVertexBuffer(GPUBufferHandle, VertexBufferBindingDesc) override {}
            void BindIndexBuffer(GPUBufferHandle, IndexElementSize, uint32_t) override {}
            void DrawIndexed(uint32_t, uint32_t, uint32_t, int32_t) override {}
            void DrawPrimitives(uint32_t, uint32_t, uint32_t) override {}
            void PushVertexUniform(uint32_t, const void*, uint32_t) override {}
            void PushFragmentUniform(uint32_t, const void*, uint32_t) override {}

            GPUComputePipelineHandle CreateComputePipeline(const ComputePipelineCreateDesc&) override
            {
                return {};
            }

            void DestroyComputePipeline(GPUComputePipelineHandle) override {}

            void BeginComputePass() override
            {
                m_events.emplace_back("BeginComputePass");
            }

            void EndComputePass() override
            {
                m_events.emplace_back("EndComputePass");
            }

            void BindComputePipeline(GPUComputePipelineHandle) override {}
            void DispatchCompute(uint32_t, uint32_t, uint32_t) override {}

            GPUTextureHandle CreateTexture(const TextureCreateDesc&) override
            {
                return GPUTextureHandle{.Index = m_nextId++, .Generation = 1};
            }

            void DestroyTexture(GPUTextureHandle) override {}
            void UploadToTexture(GPUTextureHandle, const void*, uint32_t, uint32_t, uint32_t, uint32_t) override {}
            void GenerateMipmaps(GPUTextureHandle) override {}

            GPUSamplerHandle CreateSampler(const SamplerCreateDesc&) override
            {
                return GPUSamplerHandle{.Index = m_nextId++, .Generation = 1};
            }

            void DestroySampler(GPUSamplerHandle) override {}
            void BindFragmentSampler(uint32_t, GPUTextureHandle, GPUSamplerHandle) override {}

            [[nodiscard]] Extent2D GetSwapchainDimensions() const override
            {
                return {.width = 1280, .height = 720};
            }

            const RenderDeviceInfo& GetDeviceInfo() const override
            {
                return m_info;
            }

            const std::vector<std::string>& GetEvents() const
            {
                return m_events;
            }

            const std::vector<Wayfinder::RenderPassDescriptor>& GetCapturedDescriptors() const
            {
                return m_capturedDescriptors;
            }

        private:
            RenderDeviceInfo m_info{.BackendName = "Tracking"};
            std::vector<std::string> m_events;
            std::vector<Wayfinder::RenderPassDescriptor> m_capturedDescriptors;
            std::vector<std::string> m_capturedNames;
            uint32_t m_nextId = 0;
        };
    } // namespace

    // Helper: create a NullDevice + TransientResourcePool for graph execution
    namespace
    {
        struct TrackingTestFixture
        {
            TrackingRenderDevice Device;
            Wayfinder::TransientResourcePool Pool;

            TrackingTestFixture()
            {
                Pool.Initialise(Device);
            }

            ~TrackingTestFixture()
            {
                Pool.Shutdown();
            }
        };

        struct GraphTestFixture
        {
            std::unique_ptr<Wayfinder::RenderDevice> Device;
            Wayfinder::TransientResourcePool Pool;

            GraphTestFixture() : Device(Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null))
            {
                Pool.Initialise(*Device);
            }

            ~GraphTestFixture()
            {
                Pool.Shutdown();
            }
        };
    } // anonymous namespace

    // ── Topological Sort ─────────────────────────────────────

    TEST_CASE("Empty graph does not compile")
    {
        Wayfinder::RenderGraph graph;
        CHECK_FALSE(graph.Compile());
    }

    TEST_CASE("Graph with raster passes but no swapchain writer does not compile")
    {
        Wayfinder::RenderGraph graph;
        Wayfinder::RenderGraphTextureDesc desc;
        desc.Width = 64;
        desc.Height = 64;
        desc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
        desc.DebugName = "NoPresent";

        graph.AddPass("OnlyRT", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto t = builder.CreateTransient(desc);
            builder.WriteColour(t);
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
            };
        });

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
        colourDesc.DebugName = Wayfinder::GraphTextureName(Wayfinder::GraphTextureId::SceneColour);

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
            auto colour = graph.FindHandle(Wayfinder::GraphTextureId::SceneColour);
            builder.ReadTexture(colour);
            builder.SetSwapchainOutput();
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
                executionOrder.push_back("B");
            };
        });

        REQUIRE(graph.Compile());

        GraphTestFixture fixture;
        graph.Execute(*fixture.Device, fixture.Pool);

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
        graph.Execute(*fixture.Device, fixture.Pool);

        REQUIRE(executionOrder.size() == 3);
        CHECK(executionOrder[0] == "First");
        CHECK(executionOrder[1] == "Second");
        CHECK(executionOrder[2] == "Present");
    }

    TEST_CASE("SceneColour to PostProcessColour to swapchain executes in dependency order")
    {
        std::vector<std::string> executionOrder;
        Wayfinder::RenderGraph graph;

        Wayfinder::RenderGraphTextureDesc sceneDesc;
        sceneDesc.Width = 320;
        sceneDesc.Height = 240;
        sceneDesc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
        sceneDesc.DebugName = Wayfinder::GraphTextureName(Wayfinder::GraphTextureId::SceneColour);

        graph.AddPass("A_SceneColour", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto colour = builder.CreateTransient(sceneDesc);
            builder.WriteColour(colour);
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
                executionOrder.push_back("A");
            };
        });

        graph.AddPass("B_PostProcessColour", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto scene = graph.FindHandle(Wayfinder::GraphTextureId::SceneColour);
            builder.ReadTexture(scene);

            Wayfinder::RenderGraphTextureDesc presentDesc{};
            presentDesc.Width = 320;
            presentDesc.Height = 240;
            presentDesc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
            presentDesc.DebugName = Wayfinder::GraphTextureName(Wayfinder::GraphTextureId::PostProcessColour);
            auto present = builder.CreateTransient(presentDesc);
            builder.WriteColour(present, Wayfinder::LoadOp::DontCare);

            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
                executionOrder.push_back("B");
            };
        });

        graph.AddPass("C_Composition", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto present = graph.FindHandle(Wayfinder::GraphTextureId::PostProcessColour);
            builder.ReadTexture(present);
            builder.SetSwapchainOutput();
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
                executionOrder.push_back("C");
            };
        });

        REQUIRE(graph.Compile());

        GraphTestFixture fixture;
        graph.Execute(*fixture.Device, fixture.Pool);

        REQUIRE(executionOrder.size() == 3);
        CHECK(executionOrder[0] == "A");
        CHECK(executionOrder[1] == "B");
        CHECK(executionOrder[2] == "C");
    }

    TEST_CASE("MakeCompositionUBO maps identity post-process params to composition UBO layout")
    {
        const Wayfinder::ColourGradingParams grading{};
        const Wayfinder::VignetteParams vignette{};
        const Wayfinder::ChromaticAberrationParams chromaticAberration{};
        const Wayfinder::CompositionUBO u = Wayfinder::MakeCompositionUBO(grading, vignette, chromaticAberration);
        CHECK(u.ExposureContrastSaturationPad.x == doctest::Approx(0.0f));
        CHECK(u.ExposureContrastSaturationPad.y == doctest::Approx(1.0f));
        CHECK(u.ExposureContrastSaturationPad.z == doctest::Approx(1.0f));
        CHECK(u.Lift.x == doctest::Approx(0.0f));
        CHECK(u.Gamma.x == doctest::Approx(1.0f));
        CHECK(u.Gain.x == doctest::Approx(1.0f));
        CHECK(u.VignetteAberrationPad.x == doctest::Approx(0.0f));
        CHECK(u.VignetteAberrationPad.y == doctest::Approx(0.0f));
    }

    TEST_CASE("Render graph wraps raster and compute passes in GPU debug groups")
    {
        TrackingRenderDevice device;
        Wayfinder::TransientResourcePool pool;
        pool.Initialise(device);

        Wayfinder::RenderGraph graph;

        Wayfinder::RenderGraphTextureDesc colourDesc;
        colourDesc.Width = 640;
        colourDesc.Height = 480;
        colourDesc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
        colourDesc.DebugName = "DebugColour";

        graph.AddPass("MainScene", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto colour = builder.CreateTransient(colourDesc);
            builder.WriteColour(colour);
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
            };
        });

        graph.AddComputePass("Tonemap", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto colour = graph.FindHandle("DebugColour");
            builder.ReadTexture(colour);
            builder.SetSwapchainOutput();
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
            };
        });

        REQUIRE(graph.Compile());
        graph.Execute(device, pool);

        const std::vector<std::string> expectedEvents{
            "PushDebugGroup:MainScene",
            "BeginRenderPass:MainScene",
            "EndRenderPass",
            "PopDebugGroup",
            "PushDebugGroup:Tonemap",
            "BeginComputePass",
            "EndComputePass",
            "PopDebugGroup",
        };

        CHECK(device.GetEvents() == expectedEvents);

        pool.Shutdown();
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
        graph.Execute(*fixture.Device, fixture.Pool);

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
        graph.Execute(*fixture.Device, fixture.Pool);

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
        graph.Execute(*fixture.Device, fixture.Pool);

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

    // ── Graph texture ids ────────────────────────────────────

    TEST_CASE("GraphTextureId names resolve correctly")
    {
        CHECK(Wayfinder::GraphTextures::SceneColour == Wayfinder::InternedString::Intern("SceneColour"));
        CHECK(Wayfinder::GraphTextures::SceneDepth == Wayfinder::InternedString::Intern("SceneDepth"));
        CHECK(Wayfinder::GraphTextures::PostProcessColour == Wayfinder::InternedString::Intern("PostProcessColour"));

        Wayfinder::RenderGraph graph;

        auto sceneColour = graph.ImportTexture(Wayfinder::GraphTextureId::SceneColour);
        auto sceneDepth = graph.ImportTexture(Wayfinder::GraphTextureId::SceneDepth);

        CHECK(sceneColour.IsValid());
        CHECK(sceneDepth.IsValid());
        CHECK_FALSE(sceneColour == sceneDepth);

        // FindHandle returns the same handle
        auto found = graph.FindHandle(Wayfinder::GraphTextureId::SceneColour);
        CHECK(found == sceneColour);

        // Unknown name returns invalid handle
        auto unknown = graph.FindHandle("NonExistent");
        CHECK_FALSE(unknown.IsValid());
    }

    TEST_CASE("Duplicate import returns same handle when only one resource exists")
    {
        Wayfinder::RenderGraph graph;

        auto first = graph.ImportTexture("MyTexture");
        auto second = graph.ImportTexture("MyTexture");

        CHECK(first == second);
    }

    TEST_CASE("FindHandle returns latest resource when multiple share a GraphTextureId name")
    {
        Wayfinder::RenderGraph graph;

        Wayfinder::RenderGraphTextureDesc desc{};
        desc.Width = 64;
        desc.Height = 64;
        desc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
        desc.DebugName = Wayfinder::GraphTextureName(Wayfinder::GraphTextureId::SceneColour);

        graph.AddPass("First", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto h = builder.CreateTransient(desc);
            builder.WriteColour(h);
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
            };
        });

        graph.AddPass("Second", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto h = builder.CreateTransient(desc);
            builder.WriteColour(h);
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
            };
        });

        auto latest = graph.FindHandle(Wayfinder::GraphTextureId::SceneColour);
        REQUIRE(latest.IsValid());
        CHECK(latest.Index == 1u);
    }

    TEST_CASE("ImportTexture returns latest resource when duplicate names exist")
    {
        Wayfinder::RenderGraph graph;

        Wayfinder::RenderGraphTextureDesc desc{};
        desc.Width = 64;
        desc.Height = 64;
        desc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
        desc.DebugName = "SharedName";

        graph.AddPass("First", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto h = builder.CreateTransient(desc);
            builder.WriteColour(h);
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
            };
        });

        graph.AddPass("Second", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto h = builder.CreateTransient(desc);
            builder.WriteColour(h);
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
            };
        });

        auto imported = graph.ImportTexture(Wayfinder::InternedString::Intern("SharedName"));
        REQUIRE(imported.IsValid());
        CHECK(imported.Index == 1u);
    }

    TEST_CASE("ResolvePostProcessInput returns SceneColour when no PostProcessColour exists")
    {
        Wayfinder::RenderGraph graph;

        Wayfinder::RenderGraphTextureDesc sceneDesc{};
        sceneDesc.Width = 320;
        sceneDesc.Height = 240;
        sceneDesc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
        sceneDesc.DebugName = Wayfinder::GraphTextureName(Wayfinder::GraphTextureId::SceneColour);

        graph.AddPass("Scene", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto h = builder.CreateTransient(sceneDesc);
            builder.WriteColour(h);
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
            };
        });

        const Wayfinder::RenderGraphHandle resolved = Wayfinder::ResolvePostProcessInput(graph);
        const Wayfinder::RenderGraphHandle scene = graph.FindHandle(Wayfinder::GraphTextureId::SceneColour);
        CHECK(resolved == scene);
    }

    TEST_CASE("ResolvePostProcessInput returns PostProcessColour when present")
    {
        Wayfinder::RenderGraph graph;

        Wayfinder::RenderGraphTextureDesc sceneDesc{};
        sceneDesc.Width = 320;
        sceneDesc.Height = 240;
        sceneDesc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
        sceneDesc.DebugName = Wayfinder::GraphTextureName(Wayfinder::GraphTextureId::SceneColour);

        Wayfinder::RenderGraphTextureDesc postDesc{};
        postDesc.Width = 320;
        postDesc.Height = 240;
        postDesc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
        postDesc.DebugName = Wayfinder::GraphTextureName(Wayfinder::GraphTextureId::PostProcessColour);

        graph.AddPass("Scene", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto h = builder.CreateTransient(sceneDesc);
            builder.WriteColour(h);
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
            };
        });

        graph.AddPass("Post", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto h = builder.CreateTransient(postDesc);
            builder.WriteColour(h);
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
            };
        });

        const Wayfinder::RenderGraphHandle resolved = Wayfinder::ResolvePostProcessInput(graph);
        const Wayfinder::RenderGraphHandle post = graph.FindHandle(Wayfinder::GraphTextureId::PostProcessColour);
        CHECK(resolved == post);
    }

    TEST_CASE("PostProcessColour chain: second pass reads first pass output via ResolvePostProcessInput")
    {
        Wayfinder::RenderGraph graph;

        Wayfinder::RenderGraphTextureDesc sceneDesc{};
        sceneDesc.Width = 320;
        sceneDesc.Height = 240;
        sceneDesc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
        sceneDesc.DebugName = Wayfinder::GraphTextureName(Wayfinder::GraphTextureId::SceneColour);

        Wayfinder::RenderGraphTextureDesc postDesc{};
        postDesc.Width = 320;
        postDesc.Height = 240;
        postDesc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
        postDesc.DebugName = Wayfinder::GraphTextureName(Wayfinder::GraphTextureId::PostProcessColour);

        graph.AddPass("Scene", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto h = builder.CreateTransient(sceneDesc);
            builder.WriteColour(h);
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
            };
        });

        graph.AddPass("PostA", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            builder.ReadTexture(Wayfinder::ResolvePostProcessInput(graph));
            auto h = builder.CreateTransient(postDesc);
            builder.WriteColour(h);
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
            };
        });

        graph.AddPass("PostB", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            builder.ReadTexture(Wayfinder::ResolvePostProcessInput(graph));
            auto h = builder.CreateTransient(postDesc);
            builder.WriteColour(h);
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
            };
        });

        graph.AddPass("Present", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            builder.ReadTexture(Wayfinder::ResolvePostProcessInput(graph));
            builder.SetSwapchainOutput();
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
            };
        });

        CHECK(graph.Compile());
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
        const GraphTestFixture fixture;

        // Verify NullDevice can handle all graph-related operations
        Wayfinder::RenderPassDescriptor rpDesc;
        rpDesc.debugName = "TestPass";
        rpDesc.targetSwapchain = false;

        // These should all be no-ops without crashing
        const bool passStarted = fixture.Device->BeginRenderPass(rpDesc);
        if (passStarted)
        {
            fixture.Device->EndRenderPass();
        }

        fixture.Device->BeginComputePass();
        fixture.Device->EndComputePass();

        auto tex = fixture.Device->CreateTexture({});
        fixture.Device->DestroyTexture(tex);

        auto buf = fixture.Device->CreateBuffer({});
        fixture.Device->DestroyBuffer(buf);

        auto shader = fixture.Device->CreateShader({});
        fixture.Device->DestroyShader(shader);

        const Wayfinder::PipelineCreateDesc pipelineDesc;
        auto pipeline = fixture.Device->CreatePipeline(pipelineDesc);
        fixture.Device->DestroyPipeline(pipeline);

        const Wayfinder::ComputePipelineCreateDesc computePipelineDesc;
        auto computePipeline = fixture.Device->CreateComputePipeline(computePipelineDesc);
        fixture.Device->DestroyComputePipeline(computePipeline);

        auto sampler = fixture.Device->CreateSampler({});
        fixture.Device->DestroySampler(sampler);

        const Wayfinder::Extent2D swapchainDimensions = fixture.Device->GetSwapchainDimensions();
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
        graph.Execute(*fixture.Device, fixture.Pool);

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

        TrackingTestFixture fixture;
        graph.Execute(fixture.Device, fixture.Pool);

        REQUIRE(executionOrder.size() == 2);
        CHECK(executionOrder[0] == "GBuffer");
        CHECK(executionOrder[1] == "Composition");

        // Verify the GBuffer descriptor has 2 colour targets with valid handles
        const auto& descriptors = fixture.Device.GetCapturedDescriptors();
        REQUIRE(descriptors.size() >= 1);
        const auto& gbufferDesc = descriptors[0];
        CHECK(gbufferDesc.numColourTargets == 2);
        CHECK(gbufferDesc.colourAttachments[0].target.IsValid());
        CHECK(gbufferDesc.colourAttachments[1].target.IsValid());
        CHECK_FALSE(gbufferDesc.colourAttachments[0].target == gbufferDesc.colourAttachments[1].target);
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

        TrackingTestFixture fixture;
        graph.Execute(fixture.Device, fixture.Pool);

        REQUIRE(executionOrder.size() == 3);
        CHECK(executionOrder[0] == "PassA");
        CHECK(executionOrder[1] == "PassB");
        CHECK(executionOrder[2] == "PassC");

        // PassA writes 1 colour target, PassB writes 2 (slot 0 loaded + slot 1 new)
        const auto& descriptors = fixture.Device.GetCapturedDescriptors();
        REQUIRE(descriptors.size() >= 2);
        CHECK(descriptors[0].numColourTargets == 1);
        CHECK(descriptors[0].colourAttachments[0].target.IsValid());
        CHECK(descriptors[1].numColourTargets == 2);
        CHECK(descriptors[1].colourAttachments[0].target.IsValid());
        CHECK(descriptors[1].colourAttachments[1].target.IsValid());
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

        TrackingTestFixture fixture;
        graph.Execute(fixture.Device, fixture.Pool);

        REQUIRE(executionOrder.size() == 2);
        CHECK(executionOrder[0] == "Writer");
        CHECK(executionOrder[1] == "Reader");

        // Convenience WriteColour (no slot arg) should produce slot 0
        const auto& descriptors = fixture.Device.GetCapturedDescriptors();
        REQUIRE(descriptors.size() >= 1);
        CHECK(descriptors[0].numColourTargets == 1);
        CHECK(descriptors[0].colourAttachments[0].target.IsValid());
    }

    TEST_CASE("Depth and MRT combined pass compiles and executes")
    {
        // GBuffer pass writes depth + two colour targets (albedo at slot 0, normal at slot 1).
        // Composition reads all three and writes to swapchain.
        std::vector<std::string> executionOrder;
        Wayfinder::RenderGraph graph;

        Wayfinder::RenderGraphTextureDesc albedoDesc;
        albedoDesc.Width = 1280;
        albedoDesc.Height = 720;
        albedoDesc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
        albedoDesc.DebugName = "GBuffer_Albedo";

        Wayfinder::RenderGraphTextureDesc normalDesc;
        normalDesc.Width = 1280;
        normalDesc.Height = 720;
        normalDesc.Format = Wayfinder::TextureFormat::RGBA16_FLOAT;
        normalDesc.DebugName = "GBuffer_Normal";

        Wayfinder::RenderGraphTextureDesc depthDesc;
        depthDesc.Width = 1280;
        depthDesc.Height = 720;
        depthDesc.Format = Wayfinder::TextureFormat::D32_FLOAT;
        depthDesc.DebugName = "GBuffer_Depth";

        graph.AddPass("GBuffer", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto albedo = builder.CreateTransient(albedoDesc);
            auto normal = builder.CreateTransient(normalDesc);
            auto depth = builder.CreateTransient(depthDesc);
            builder.WriteColour(albedo, 0);
            builder.WriteColour(normal, 1);
            builder.WriteDepth(depth);
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
                executionOrder.push_back("GBuffer");
            };
        });

        graph.AddPass("Lighting", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto albedo = graph.FindHandle("GBuffer_Albedo");
            auto normal = graph.FindHandle("GBuffer_Normal");
            auto depth = graph.FindHandle("GBuffer_Depth");
            builder.ReadTexture(albedo);
            builder.ReadTexture(normal);
            builder.ReadTexture(depth);
            builder.SetSwapchainOutput();
            return [&executionOrder](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
                executionOrder.push_back("Lighting");
            };
        });

        REQUIRE(graph.Compile());

        TrackingTestFixture fixture;
        graph.Execute(fixture.Device, fixture.Pool);

        REQUIRE(executionOrder.size() == 2);
        CHECK(executionOrder[0] == "GBuffer");
        CHECK(executionOrder[1] == "Lighting");

        // Verify GBuffer descriptor: 2 colour targets + depth
        const auto& descriptors = fixture.Device.GetCapturedDescriptors();
        REQUIRE(descriptors.size() >= 1);
        const auto& gbufferDesc = descriptors[0];
        CHECK(gbufferDesc.numColourTargets == 2);
        CHECK(gbufferDesc.colourAttachments[0].target.IsValid());
        CHECK(gbufferDesc.colourAttachments[1].target.IsValid());
        CHECK_FALSE(gbufferDesc.colourAttachments[0].target == gbufferDesc.colourAttachments[1].target);
        CHECK(gbufferDesc.depthAttachment.enabled);
        CHECK(gbufferDesc.depthTarget.IsValid());
    }
}

// NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access, modernize-use-emplace, modernize-use-nodiscard)
