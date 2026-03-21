#include "rendering/backend/RenderDevice.h"
#include "rendering/graph/RenderFeature.h"
#include "rendering/graph/RenderFrame.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/pipeline/Renderer.h"
#include "rendering/resources/RenderResources.h"
#include "rendering/resources/TransientResourcePool.h"

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <vector>

namespace Wayfinder::Tests
{
    // A minimal test feature that records when AddPasses is called.
    class TestFeature : public Wayfinder::RenderFeature
    {
    public:
        explicit TestFeature(std::string name, std::vector<std::string>& log)
            : m_name(std::move(name)), m_log(log) {}

        const std::string& GetName() const override { return m_name; }

        void AddPasses(Wayfinder::RenderGraph& graph, const Wayfinder::RenderFrame&) override
        {
            m_log.push_back(m_name + "::AddPasses");

            // Inject a simple pass that reads SceneColour and writes swapchain
            graph.AddPass(m_name, [this](Wayfinder::RenderGraphBuilder& builder) {
                auto colour = builder.CreateTransient({128, 128, Wayfinder::TextureFormat::RGBA8_UNORM, m_name.c_str()});
                builder.WriteColour(colour);
                return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&) {};
            });
        }

        void OnAttach(const Wayfinder::RenderFeatureContext&) override
        {
            m_log.push_back(m_name + "::OnAttach");
        }

        void OnDetach(const Wayfinder::RenderFeatureContext&) override
        {
            m_log.push_back(m_name + "::OnDetach");
        }

    private:
        std::string m_name;
        std::vector<std::string>& m_log;
    };

    // A feature that injects a pass reading SceneColour and writing to swapchain.
    class OverlayFeature : public Wayfinder::RenderFeature
    {
    public:
        const std::string& GetName() const override
        {
            static const std::string name = "Overlay";
            return name;
        }

        void AddPasses(Wayfinder::RenderGraph& graph, const Wayfinder::RenderFrame&) override
        {
            graph.AddPass("OverlayPass", [&](Wayfinder::RenderGraphBuilder& builder) {
                auto colour = graph.FindHandle(Wayfinder::WellKnown::SceneColour);
                if (colour.IsValid())
                {
                    builder.ReadTexture(colour);
                }
                builder.SetSwapchainOutput();
                return [this](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&) {
                    m_executed = true;
                };
            });
        }

        bool WasExecuted() const { return m_executed; }
        void Reset() { m_executed = false; }

    private:
        bool m_executed = false;
    };

    // ── Feature Lifecycle ────────────────────────────────────

    TEST_CASE("RenderFeature default state")
    {
        std::vector<std::string> log;
        TestFeature feature("Test", log);

        CHECK(feature.IsEnabled());
        CHECK(feature.GetName() == "Test");

        feature.SetEnabled(false);
        CHECK_FALSE(feature.IsEnabled());
    }

    // ── Feature Injection into Graph ─────────────────────────

    TEST_CASE("Feature injects passes into render graph")
    {
        std::vector<std::string> log;
        TestFeature feature("MyEffect", log);
        Wayfinder::RenderFrame frame;
        Wayfinder::RenderGraph graph;

        feature.AddPasses(graph, frame);

        CHECK(log.size() == 1);
        CHECK(log[0] == "MyEffect::AddPasses");
    }

    TEST_CASE("Disabled feature can be skipped by caller")
    {
        std::vector<std::string> log;
        TestFeature feature("Skipped", log);
        feature.SetEnabled(false);

        Wayfinder::RenderFrame frame;
        Wayfinder::RenderGraph graph;

        // The renderer checks IsEnabled() before calling AddPasses.
        // We simulate that pattern here.
        if (feature.IsEnabled())
        {
            feature.AddPasses(graph, frame);
        }

        CHECK(log.empty());
    }

    // ── Multiple Features ────────────────────────────────────

    TEST_CASE("Multiple features inject passes in registration order")
    {
        std::vector<std::string> log;

        auto featureA = std::make_unique<TestFeature>("FeatureA", log);
        auto featureB = std::make_unique<TestFeature>("FeatureB", log);

        std::vector<std::unique_ptr<Wayfinder::RenderFeature>> features;
        features.push_back(std::move(featureA));
        features.push_back(std::move(featureB));

        Wayfinder::RenderFrame frame;
        Wayfinder::RenderGraph graph;

        for (auto& feature : features)
        {
            if (feature->IsEnabled())
            {
                feature->AddPasses(graph, frame);
            }
        }

        REQUIRE(log.size() == 2);
        CHECK(log[0] == "FeatureA::AddPasses");
        CHECK(log[1] == "FeatureB::AddPasses");
    }

    // ── Feature With Graph Execution ─────────────────────────

    TEST_CASE("Feature pass executes in compiled graph")
    {
        auto device = Wayfinder::RenderDevice::Create(Wayfinder::RenderBackend::Null);
        Wayfinder::TransientResourcePool pool;
        pool.Initialise(*device);

        OverlayFeature overlay;
        Wayfinder::RenderFrame frame;
        Wayfinder::RenderGraph graph;

        // Engine adds a scene pass first
        Wayfinder::RenderGraphTextureDesc colourDesc;
        colourDesc.Width = 800;
        colourDesc.Height = 600;
        colourDesc.Format = Wayfinder::TextureFormat::RGBA8_UNORM;
        colourDesc.DebugName = Wayfinder::WellKnown::SceneColour;

        graph.AddPass("MainScene", [&](Wayfinder::RenderGraphBuilder& builder) {
            auto colour = builder.CreateTransient(colourDesc);
            builder.WriteColour(colour);
            return [](Wayfinder::RenderDevice&, const Wayfinder::RenderGraphResources&) {};
        });

        // Feature injects its pass
        overlay.AddPasses(graph, frame);

        REQUIRE(graph.Compile());
        graph.Execute(*device, pool);

        CHECK(overlay.WasExecuted());

        pool.Shutdown();
    }

    TEST_CASE("Removing a feature stops its pass injection")
    {
        std::vector<std::string> log;

        std::vector<std::unique_ptr<Wayfinder::RenderFeature>> features;
        features.push_back(std::make_unique<TestFeature>("Removable", log));
        features.push_back(std::make_unique<TestFeature>("Persistent", log));

        // Remove the first feature (simulate Renderer::RemoveFeature)
        features.erase(features.begin());

        Wayfinder::RenderFrame frame;
        Wayfinder::RenderGraph graph;

        for (auto& feature : features)
        {
            if (feature->IsEnabled())
            {
                feature->AddPasses(graph, frame);
            }
        }

        REQUIRE(log.size() == 1);
        CHECK(log[0] == "Persistent::AddPasses");
    }

    // ── Pipeline and Buffer Headless Tests ───────────────────

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

        // Upload some data — should be a no-op on NullDevice
        uint8_t data[64] = {};
        device->UploadToBuffer(buffer, data, sizeof(data));

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

    // ── Material Resolution ──────────────────────────────────

    TEST_CASE("RenderResourceCache resolves built-in materials")
    {
        Wayfinder::RenderFrame frame;
        const size_t viewIndex = frame.AddView(Wayfinder::RenderView{});
        Wayfinder::RenderPass& scenePass =
            frame.AddScenePass(Wayfinder::RenderPassIds::MainScene, viewIndex, Wayfinder::RenderLayers::Main);

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

        // Verify that the submission's material binding was set up correctly
        CHECK(scenePass.Meshes[0].Material.Ref.Origin == Wayfinder::RenderResourceOrigin::BuiltIn);
        CHECK(scenePass.Meshes[0].Material.ShaderName == "unlit");
        CHECK(scenePass.Meshes[0].Material.Parameters.Has("base_colour"));
    }
}
