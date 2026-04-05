#include "app/EngineConfig.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/graph/RenderFeature.h"
#include "rendering/graph/RenderFrame.h"
#include "rendering/graph/RenderFrameUtils.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/pipeline/BuiltInUBOs.h"
#include "rendering/pipeline/FrameRenderParams.h"
#include "rendering/pipeline/RenderOrchestrator.h"
#include "rendering/pipeline/RenderServices.h"

#include <doctest/doctest.h>

#include <memory>
#include <string>

// NOLINTBEGIN(misc-const-correctness, misc-use-internal-linkage, modernize-use-nodiscard, readability-identifier-naming)

namespace Wayfinder::Tests
{
    namespace
    {
        Wayfinder::FrameRenderParams MakeTestFrameParams(Wayfinder::RenderFrame& frame, uint32_t w = 800, uint32_t h = 600)
        {
            static const Wayfinder::BuiltInMeshTable K_EMPTY_MESHES{};
            return Wayfinder::FrameRenderParams{
                .Frame = frame,
                .SwapchainWidth = w,
                .SwapchainHeight = h,
                .BuiltInMeshes = K_EMPTY_MESHES,
                .ResourceCache = nullptr,
                .PrimaryView = Wayfinder::Rendering::ResolvePreparedPrimaryView(frame),
            };
        }

        /// Test feature that tracks AddPasses invocations.
        class TrackingFeature final : public Wayfinder::RenderFeature
        {
        public:
            explicit TrackingFeature(std::string name) : m_name(std::move(name)) {}

            std::string_view GetName() const override
            {
                return m_name;
            }

            void AddPasses(Wayfinder::RenderGraph& /*graph*/, const Wayfinder::FrameRenderParams& /*params*/) override
            {
                ++AddPassesCallCount;
            }

            int AddPassesCallCount = 0;

        private:
            std::string m_name;
        };

        /// Helper: create a null-backed RenderServices + RenderOrchestrator, initialised.
        struct OrchestratorFixture
        {
            std::unique_ptr<RenderDevice> Device;
            RenderServices Services;
            RenderOrchestrator Orchestrator;

            OrchestratorFixture()
            {
                Device = RenderDevice::Create(RenderBackend::Null);
                EngineConfig config;
                config.Window.Width = 320;
                config.Window.Height = 240;
                (void)Services.Initialise(*Device, config);
                Orchestrator.Initialise(Services);
            }

            ~OrchestratorFixture()
            {
                Orchestrator.Shutdown();
                Services.Shutdown();
            }
        };
    } // namespace

    TEST_SUITE("RenderFeature Capability Gating")
    {
        TEST_CASE("RenderFeature SetEnabled defaults to true")
        {
            TrackingFeature feature("DefaultEnabled");
            CHECK(feature.IsEnabled());
        }

        TEST_CASE("RenderFeature SetEnabled(false) disables feature")
        {
            TrackingFeature feature("Disabled");
            feature.SetEnabled(false);
            CHECK_FALSE(feature.IsEnabled());
        }

        TEST_CASE("Disabled render feature skips AddPasses in BuildGraph")
        {
            OrchestratorFixture fixture;

            auto enabledFeature = std::make_unique<TrackingFeature>("Enabled");
            auto disabledFeature = std::make_unique<TrackingFeature>("Disabled");
            disabledFeature->SetEnabled(false);

            auto* enabledPtr = enabledFeature.get();
            auto* disabledPtr = disabledFeature.get();

            fixture.Orchestrator.RegisterFeature(RenderPhase::Opaque, 0, std::move(enabledFeature));
            fixture.Orchestrator.RegisterFeature(RenderPhase::Opaque, 1, std::move(disabledFeature));

            RenderFrame frame;
            RenderGraph graph;

            fixture.Orchestrator.BuildGraph(graph, MakeTestFrameParams(frame));

            CHECK(enabledPtr->AddPassesCallCount == 1);
            CHECK(disabledPtr->AddPassesCallCount == 0);
        }

        TEST_CASE("SetEnabled toggles independently per feature")
        {
            TrackingFeature featureA("FeatureA");
            TrackingFeature featureB("FeatureB");

            featureA.SetEnabled(false);

            CHECK_FALSE(featureA.IsEnabled());
            CHECK(featureB.IsEnabled());

            featureA.SetEnabled(true);
            featureB.SetEnabled(false);

            CHECK(featureA.IsEnabled());
            CHECK_FALSE(featureB.IsEnabled());
        }

        TEST_CASE("SetEnabled round-trip preserves lifecycle")
        {
            OrchestratorFixture fixture;

            auto feature = std::make_unique<TrackingFeature>("RoundTrip");
            auto* featurePtr = feature.get();
            fixture.Orchestrator.RegisterFeature(RenderPhase::PostProcess, 0, std::move(feature));

            RenderFrame frame;

            // Initially enabled: AddPasses called.
            RenderGraph graph1;
            fixture.Orchestrator.BuildGraph(graph1, MakeTestFrameParams(frame));
            CHECK(featurePtr->AddPassesCallCount == 1);

            // Disable: AddPasses not called.
            featurePtr->SetEnabled(false);
            RenderGraph graph2;
            fixture.Orchestrator.BuildGraph(graph2, MakeTestFrameParams(frame));
            CHECK(featurePtr->AddPassesCallCount == 1); // Still 1, not incremented.

            // Re-enable: AddPasses called again.
            featurePtr->SetEnabled(true);
            RenderGraph graph3;
            fixture.Orchestrator.BuildGraph(graph3, MakeTestFrameParams(frame));
            CHECK(featurePtr->AddPassesCallCount == 2);
        }

        TEST_CASE("Disabled features are still attached (lifecycle unchanged)")
        {
            TrackingFeature feature("StillAttached");
            feature.SetEnabled(false);

            // Feature is disabled but still a valid object with accessible state.
            CHECK_FALSE(feature.IsEnabled());
            CHECK(feature.GetName() == "StillAttached");
            CHECK(feature.AddPassesCallCount == 0);

            // Re-enable works without re-attach.
            feature.SetEnabled(true);
            CHECK(feature.IsEnabled());
        }
    }

} // namespace Wayfinder::Tests

// NOLINTEND(misc-const-correctness, misc-use-internal-linkage, modernize-use-nodiscard, readability-identifier-naming)
