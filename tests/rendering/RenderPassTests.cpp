#include "app/EngineConfig.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/graph/RenderFrame.h"
#include "rendering/graph/RenderFrameUtils.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/graph/RenderPass.h"
#include "rendering/pipeline/RenderPipelineFrameParams.h"
#include "rendering/pipeline/Renderer.h"
#include "rendering/pipeline/ShaderUniforms.h"
#include "rendering/resources/RenderResources.h"
#include "rendering/resources/TransientResourcePool.h"

#include <doctest/doctest.h>

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Test doubles and doctest CHECK patterns (file-wide suppressions for test code).
// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays, cppcoreguidelines-avoid-const-or-ref-data-members, cppcoreguidelines-pro-bounds-array-to-pointer-decay, cppcoreguidelines-pro-bounds-avoid-unchecked-container-access,
// misc-const-correctness, misc-use-internal-linkage, modernize-avoid-c-arrays, modernize-use-designated-initializers, modernize-use-nodiscard, readability-identifier-naming)

namespace Wayfinder::Tests
{
    namespace
    {
        Wayfinder::RenderPipelineFrameParams MakeTestParams(Wayfinder::RenderFrame& frame, uint32_t w = 800, uint32_t h = 600)
        {
            static const std::unordered_map<uint32_t, Wayfinder::Mesh*> K_EMPTY_MESHES;
            return Wayfinder::RenderPipelineFrameParams{
                .Frame = frame,
                .SwapchainWidth = w,
                .SwapchainHeight = h,
                .MeshesByStride = K_EMPTY_MESHES,
                .ResourceCache = nullptr,
                .PrimaryView = Wayfinder::Rendering::ResolvePreparedPrimaryView(frame),
            };
        }

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

            bool BeginRenderPass(const Wayfinder::RenderPassDescriptor&) override
            {
                return true;
            }
            void EndRenderPass() override {}

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
            void BeginComputePass() override {}
            void EndComputePass() override {}
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
                return {.width = 320, .height = 240};
            }

            const RenderDeviceInfo& GetDeviceInfo() const override
            {
                return m_info;
            }

            const std::vector<std::string>& GetEvents() const
            {
                return m_events;
            }

        private:
            RenderDeviceInfo m_info{.BackendName = "Tracking"};
            std::vector<std::string> m_events;
            uint32_t m_nextId = 0;
        };
    } // namespace

    class TestPass : public Wayfinder::RenderPass
    {
    public:
        explicit TestPass(std::string name, std::vector<std::string>& log) : m_name(std::move(name)), m_log(log) {}

        std::string_view GetName() const override
        {
            return m_name;
        }

        void AddPasses(Wayfinder::RenderGraph& graph, const Wayfinder::RenderPipelineFrameParams& /*params*/) override
        {
            m_log.push_back(std::string(m_name) + "::AddPasses");

            graph.AddPass(m_name, [this](Wayfinder::RenderGraphBuilder& builder)
            {
                auto colour = builder.CreateTransient({128, 128, Wayfinder::TextureFormat::RGBA8_UNORM, m_name.c_str()});
                builder.WriteColour(colour);
                return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
                {
                };
            });
        }

        void OnAttach(const Wayfinder::RenderPassContext&) override
        {
            m_log.push_back(std::string(m_name) + "::OnAttach");
        }

        void OnDetach(const Wayfinder::RenderPassContext&) override
        {
            m_log.push_back(std::string(m_name) + "::OnDetach");
        }

    private:
        std::string m_name;
        std::vector<std::string>& m_log;
    };

    class OverlayPass : public Wayfinder::RenderPass
    {
    public:
        std::string_view GetName() const override
        {
            return "Overlay";
        }

        void AddPasses(Wayfinder::RenderGraph& graph, const Wayfinder::RenderPipelineFrameParams& /*params*/) override
        {
            graph.AddPass("OverlayPass", [&](Wayfinder::RenderGraphBuilder& builder)
            {
                auto colour = graph.FindHandle(Wayfinder::GraphTextureId::SceneColour);
                if (colour.IsValid())
                {
                    builder.ReadTexture(colour);
                }
                builder.SetSwapchainOutput();
                return [this](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
                {
                    m_executed = true;
                };
            });
        }

        bool WasExecuted() const
        {
            return m_executed;
        }
        void Reset()
        {
            m_executed = false;
        }

    private:
        bool m_executed = false;
    };

    TEST_CASE("RenderPass default state")
    {
        std::vector<std::string> log;
        TestPass pass("Test", log);

        CHECK(pass.IsEnabled());
        CHECK(std::string(pass.GetName()) == "Test");

        pass.SetEnabled(false);
        CHECK_FALSE(pass.IsEnabled());
    }

    TEST_CASE("Pass injects passes into render graph")
    {
        std::vector<std::string> log;
        TestPass pass("MyEffect", log);
        Wayfinder::RenderFrame frame;
        Wayfinder::RenderGraph graph;

        pass.AddPasses(graph, MakeTestParams(frame));

        CHECK(log.size() == 1);
        CHECK(log[0] == "MyEffect::AddPasses");
    }

    TEST_CASE("Disabled pass can be skipped by caller")
    {
        std::vector<std::string> log;
        TestPass pass("Skipped", log);
        pass.SetEnabled(false);

        Wayfinder::RenderFrame frame;
        Wayfinder::RenderGraph graph;

        if (pass.IsEnabled())
        {
            pass.AddPasses(graph, MakeTestParams(frame));
        }

        CHECK(log.empty());
    }

    TEST_CASE("Multiple passes inject in registration order")
    {
        std::vector<std::string> log;

        auto passA = std::make_unique<TestPass>("PassA", log);
        auto passB = std::make_unique<TestPass>("PassB", log);

        std::vector<std::unique_ptr<Wayfinder::RenderPass>> passes;
        passes.push_back(std::move(passA));
        passes.push_back(std::move(passB));

        Wayfinder::RenderFrame frame;
        Wayfinder::RenderGraph graph;

        for (auto& p : passes)
        {
            if (p->IsEnabled())
            {
                p->AddPasses(graph, MakeTestParams(frame));
            }
        }

        REQUIRE(log.size() == 2);
        CHECK(log[0] == "PassA::AddPasses");
        CHECK(log[1] == "PassB::AddPasses");
    }

    TEST_CASE("Pass executes in compiled graph")
    {
        auto device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null);
        Wayfinder::TransientResourcePool pool;
        pool.Initialise(*device);

        OverlayPass overlay;
        Wayfinder::RenderFrame frame;
        Wayfinder::RenderGraph graph;

        Wayfinder::RenderGraphTextureDesc colourDesc;
        colourDesc.Width = 800;
        colourDesc.Height = 600;
        colourDesc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
        colourDesc.DebugName = Wayfinder::GraphTextureName(Wayfinder::GraphTextureId::SceneColour);

        graph.AddPass("MainScene", [&](Wayfinder::RenderGraphBuilder& builder)
        {
            auto colour = builder.CreateTransient(colourDesc);
            builder.WriteColour(colour);
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&)
            {
            };
        });

        overlay.AddPasses(graph, MakeTestParams(frame));

        REQUIRE(graph.Compile());
        graph.Execute(*device, pool);

        CHECK(overlay.WasExecuted());

        pool.Shutdown();
    }

    TEST_CASE("Renderer wraps the frame in a GPU debug group")
    {
        TrackingRenderDevice device;
        Wayfinder::EngineConfig config;
        config.Window.Width = 320;
        config.Window.Height = 240;

        Wayfinder::Renderer renderer;
        REQUIRE(renderer.Initialise(device, config));

        Wayfinder::RenderFrame frame;
        frame.SceneName = "Empty";

        renderer.Render(frame);
        renderer.Shutdown();

        REQUIRE(device.GetEvents().size() >= 4);
        CHECK(device.GetEvents()[0] == "BeginFrame");
        CHECK(device.GetEvents()[1] == "PushDebugGroup:Frame: Empty");
        CHECK(device.GetEvents()[2] == "PopDebugGroup");
        CHECK(device.GetEvents()[3] == "EndFrame");
    }

    TEST_CASE("Removing a pass stops its pass injection")
    {
        std::vector<std::string> log;

        std::vector<std::unique_ptr<Wayfinder::RenderPass>> passes;
        passes.push_back(std::make_unique<TestPass>("Removable", log));
        passes.push_back(std::make_unique<TestPass>("Persistent", log));

        passes.erase(passes.begin());

        Wayfinder::RenderFrame frame;
        Wayfinder::RenderGraph graph;

        for (auto& p : passes)
        {
            if (p->IsEnabled())
            {
                p->AddPasses(graph, MakeTestParams(frame));
            }
        }

        REQUIRE(log.size() == 1);
        CHECK(log[0] == "Persistent::AddPasses");
    }

    TEST_CASE("NullDevice pipeline creation returns handle")
    {
        auto device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null);

        Wayfinder::PipelineCreateDesc desc;
        auto pipeline = device->CreatePipeline(desc);
        CHECK_FALSE(pipeline.IsValid());
        device->DestroyPipeline(pipeline);
    }

    TEST_CASE("NullDevice buffer upload does not crash")
    {
        auto device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null);

        Wayfinder::BufferCreateDesc desc;
        desc.usage = Wayfinder::BufferUsage::Vertex;
        desc.sizeInBytes = 1024;

        auto buffer = device->CreateBuffer(desc);
        CHECK_FALSE(buffer.IsValid());

        std::array<uint8_t, 64> data{};
        device->UploadToBuffer(buffer, data.data(), {.sizeInBytes = static_cast<uint32_t>(data.size())});

        device->DestroyBuffer(buffer);
    }

    TEST_CASE("NullDevice shader creation returns handle")
    {
        auto device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null);

        Wayfinder::ShaderCreateDesc desc;
        desc.stage = Wayfinder::ShaderStage::Vertex;
        desc.entryPoint = "main";

        auto shader = device->CreateShader(desc);
        CHECK_FALSE(shader.IsValid());
        device->DestroyShader(shader);
    }

    TEST_CASE("RenderResourceCache resolves built-in materials")
    {
        Wayfinder::RenderFrame frame;
        const size_t viewIndex = frame.AddView(Wayfinder::RenderView{});
        Wayfinder::FrameLayerRecord& scenePass = frame.AddSceneLayer(Wayfinder::FrameLayerIds::MainScene, viewIndex, Wayfinder::RenderLayers::Main);

        Wayfinder::RenderMeshSubmission submission;
        submission.Mesh.Origin = Wayfinder::RenderResourceOrigin::BuiltIn;
        submission.Mesh.StableKey = 42;
        submission.Geometry.Type = Wayfinder::RenderGeometryType::Box;
        submission.Material.Ref.Origin = Wayfinder::RenderResourceOrigin::BuiltIn;
        submission.Material.Ref.StableKey = 42;
        submission.Material.ShaderName = "unlit";
        submission.Material.Parameters.SetColour("base_colour", Wayfinder::LinearColour::White());
        scenePass.Meshes.push_back(submission);

        Wayfinder::RenderResourceCache resources;
        resources.PrepareFrame(frame);

        const auto& resolved = resources.ResolveMesh(scenePass.Meshes[0]);
        CHECK(resolved.Geometry.Type == Wayfinder::RenderGeometryType::Box);
        CHECK(resolved.Ref.Origin == Wayfinder::RenderResourceOrigin::BuiltIn);
        CHECK(resolved.Ref.StableKey == 42);

        CHECK(scenePass.Meshes[0].Material.Ref.Origin == Wayfinder::RenderResourceOrigin::BuiltIn);
        CHECK(scenePass.Meshes[0].Material.ShaderName == "unlit");
        CHECK(scenePass.Meshes[0].Material.Parameters.Has("base_colour"));
    }

    TEST_CASE("ResolvePreparedPrimaryView is invalid when there are no prepared views")
    {
        Wayfinder::RenderFrame frame;
        CHECK_FALSE(Wayfinder::Rendering::ResolvePreparedPrimaryView(frame).Valid);
    }

    TEST_CASE("ResolvePreparedPrimaryView is valid when the first view is prepared")
    {
        Wayfinder::RenderFrame frame;
        Wayfinder::RenderView view;
        view.Prepared = true;
        view.ViewMatrix = Wayfinder::Matrix4(1.0f);
        view.ProjectionMatrix = Wayfinder::Matrix4(1.0f);
        frame.Views.push_back(view);
        const Wayfinder::Rendering::PreparedPrimaryView pv = Wayfinder::Rendering::ResolvePreparedPrimaryView(frame);
        CHECK(pv.Valid);
    }

    TEST_CASE("SceneGlobalsUBO size matches std140 contract")
    {
        CHECK(sizeof(Wayfinder::SceneGlobalsUBO) == 32);
    }
} // namespace Wayfinder::Tests

// NOLINTEND(cppcoreguidelines-avoid-c-arrays, cppcoreguidelines-avoid-const-or-ref-data-members, cppcoreguidelines-pro-bounds-array-to-pointer-decay, cppcoreguidelines-pro-bounds-avoid-unchecked-container-access,
// misc-const-correctness, misc-use-internal-linkage, modernize-avoid-c-arrays, modernize-use-designated-initializers, modernize-use-nodiscard, readability-identifier-naming)
