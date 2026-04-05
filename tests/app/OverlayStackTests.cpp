#include "app/OverlayStack.h"

#include "app/EngineContext.h"
#include "app/IOverlay.h"
#include "app/OrchestrationTypes.h"
#include "core/events/EventQueue.h"
#include "gameplay/Capability.h"
#include "gameplay/NativeTag.h"
#include "gameplay/Tag.h"
#include "gameplay/TagRegistry.h"

#include <doctest/doctest.h>

namespace Wayfinder::Tests
{
    // -- Static counters for ordering verification ------------------------

    static int s_orderCounter = 0;

    // -- Mock overlays ----------------------------------------------------

    class TestOverlayA : public IOverlay
    {
    public:
        bool AttachCalled = false;
        bool DetachCalled = false;
        bool UpdateCalled = false;
        bool RenderCalled = false;
        bool EventCalled = false;
        bool ShouldConsumeEvents = false;
        int UpdateOrder = 0;
        int RenderOrder = 0;
        int EventOrder = 0;

        auto OnAttach(EngineContext& /*ctx*/) -> Result<void> override
        {
            AttachCalled = true;
            return {};
        }

        auto OnDetach(EngineContext& /*ctx*/) -> Result<void> override
        {
            DetachCalled = true;
            return {};
        }

        void OnUpdate(EngineContext& /*ctx*/, float /*dt*/) override
        {
            UpdateCalled = true;
            UpdateOrder = ++s_orderCounter;
        }

        void OnRender(EngineContext& /*ctx*/) override
        {
            RenderCalled = true;
            RenderOrder = ++s_orderCounter;
        }

        auto OnEvent(EngineContext& /*ctx*/, EventQueue& /*events*/) -> bool override
        {
            EventCalled = true;
            EventOrder = ++s_orderCounter;
            return ShouldConsumeEvents;
        }

        [[nodiscard]] auto GetName() const -> std::string_view override
        {
            return "TestOverlayA";
        }
    };

    class TestOverlayB : public IOverlay
    {
    public:
        bool AttachCalled = false;
        bool DetachCalled = false;
        bool UpdateCalled = false;
        bool RenderCalled = false;
        bool EventCalled = false;
        bool ShouldConsumeEvents = false;
        int UpdateOrder = 0;
        int RenderOrder = 0;
        int EventOrder = 0;

        auto OnAttach(EngineContext& /*ctx*/) -> Result<void> override
        {
            AttachCalled = true;
            return {};
        }

        auto OnDetach(EngineContext& /*ctx*/) -> Result<void> override
        {
            DetachCalled = true;
            return {};
        }

        void OnUpdate(EngineContext& /*ctx*/, float /*dt*/) override
        {
            UpdateCalled = true;
            UpdateOrder = ++s_orderCounter;
        }

        void OnRender(EngineContext& /*ctx*/) override
        {
            RenderCalled = true;
            RenderOrder = ++s_orderCounter;
        }

        auto OnEvent(EngineContext& /*ctx*/, EventQueue& /*events*/) -> bool override
        {
            EventCalled = true;
            EventOrder = ++s_orderCounter;
            return ShouldConsumeEvents;
        }

        [[nodiscard]] auto GetName() const -> std::string_view override
        {
            return "TestOverlayB";
        }
    };

    class TestOverlayC : public IOverlay
    {
    public:
        bool AttachCalled = false;
        bool DetachCalled = false;
        bool UpdateCalled = false;
        bool RenderCalled = false;
        bool EventCalled = false;
        bool ShouldConsumeEvents = false;
        int UpdateOrder = 0;
        int RenderOrder = 0;
        int EventOrder = 0;

        auto OnAttach(EngineContext& /*ctx*/) -> Result<void> override
        {
            AttachCalled = true;
            return {};
        }

        auto OnDetach(EngineContext& /*ctx*/) -> Result<void> override
        {
            DetachCalled = true;
            return {};
        }

        void OnUpdate(EngineContext& /*ctx*/, float /*dt*/) override
        {
            UpdateCalled = true;
            UpdateOrder = ++s_orderCounter;
        }

        void OnRender(EngineContext& /*ctx*/) override
        {
            RenderCalled = true;
            RenderOrder = ++s_orderCounter;
        }

        auto OnEvent(EngineContext& /*ctx*/, EventQueue& /*events*/) -> bool override
        {
            EventCalled = true;
            EventOrder = ++s_orderCounter;
            return ShouldConsumeEvents;
        }

        [[nodiscard]] auto GetName() const -> std::string_view override
        {
            return "TestOverlayC";
        }
    };

    // -- Fixture ----------------------------------------------------------

    struct OverlayFixture
    {
        TagRegistry Registry;
        EngineContext Context;
        EventQueue Events;
        OverlayStack Stack;

        TestOverlayA OverlayA;
        TestOverlayB OverlayB;
        TestOverlayC OverlayC;

        OverlayFixture()
        {
            NativeTag::RegisterAll(Registry);
            s_orderCounter = 0;
        }

        /// Add all three overlays with default descriptors and mark caps satisfied.
        void AddDefaultOverlays()
        {
            Stack.AddOverlay(&OverlayA, std::type_index(typeid(TestOverlayA)), {}, 0);
            Stack.AddOverlay(&OverlayB, std::type_index(typeid(TestOverlayB)), {}, 1);
            Stack.AddOverlay(&OverlayC, std::type_index(typeid(TestOverlayC)), {}, 2);
            // Empty RequiredCapabilities means always satisfied.
            Stack.UpdateCapabilities({}, Context);
        }
    };

    // -- Tests ------------------------------------------------------------

    TEST_SUITE("OverlayStack")
    {
        TEST_CASE_FIXTURE(OverlayFixture, "Overlays execute in registration order for update")
        {
            AddDefaultOverlays();
            Stack.Update(Context, 0.016f);

            CHECK(OverlayA.UpdateCalled);
            CHECK(OverlayB.UpdateCalled);
            CHECK(OverlayC.UpdateCalled);
            CHECK(OverlayA.UpdateOrder < OverlayB.UpdateOrder);
            CHECK(OverlayB.UpdateOrder < OverlayC.UpdateOrder);
        }

        TEST_CASE_FIXTURE(OverlayFixture, "Overlays render in registration order (bottom-up)")
        {
            AddDefaultOverlays();
            Stack.Render(Context);

            CHECK(OverlayA.RenderCalled);
            CHECK(OverlayB.RenderCalled);
            CHECK(OverlayC.RenderCalled);
            CHECK(OverlayA.RenderOrder < OverlayB.RenderOrder);
            CHECK(OverlayB.RenderOrder < OverlayC.RenderOrder);
        }

        TEST_CASE_FIXTURE(OverlayFixture, "Event processing is top-down (reverse order)")
        {
            AddDefaultOverlays();
            Stack.ProcessEvents(Context, Events);

            CHECK(OverlayA.EventCalled);
            CHECK(OverlayB.EventCalled);
            CHECK(OverlayC.EventCalled);
            // C is highest index/priority, processed first in reverse.
            CHECK(OverlayC.EventOrder < OverlayB.EventOrder);
            CHECK(OverlayB.EventOrder < OverlayA.EventOrder);
        }

        TEST_CASE_FIXTURE(OverlayFixture, "Event consumption stops propagation")
        {
            AddDefaultOverlays();
            OverlayB.ShouldConsumeEvents = true;

            auto consumed = Stack.ProcessEvents(Context, Events);

            CHECK(consumed);
            CHECK(OverlayC.EventCalled);       // Checked first (highest priority).
            CHECK(OverlayB.EventCalled);       // Consumed.
            CHECK_FALSE(OverlayA.EventCalled); // Propagation stopped.
        }

        TEST_CASE_FIXTURE(OverlayFixture, "Overlay with empty RequiredCapabilities is always satisfied")
        {
            Stack.AddOverlay(&OverlayA, std::type_index(typeid(TestOverlayA)), {}, 0);
            Stack.UpdateCapabilities({}, Context);

            auto entries = Stack.GetEntries();
            REQUIRE(entries.size() == 1);
            CHECK(entries[0].CapabilitySatisfied);
            CHECK(entries[0].IsActive());
        }

        TEST_CASE_FIXTURE(OverlayFixture, "Overlay activates when capabilities become satisfied")
        {
            OverlayDescriptor desc;
            desc.RequiredCapabilities.AddTag(Capability::Rendering);
            Stack.AddOverlay(&OverlayA, std::type_index(typeid(TestOverlayA)), desc, 0);

            // Initially no caps - not satisfied.
            Stack.UpdateCapabilities({}, Context);
            CHECK_FALSE(OverlayA.AttachCalled);
            CHECK_FALSE(Stack.GetEntries()[0].CapabilitySatisfied);

            // Provide the required capability.
            CapabilitySet caps;
            caps.AddTag(Capability::Rendering);
            Stack.UpdateCapabilities(caps, Context);
            CHECK(OverlayA.AttachCalled);
            CHECK(Stack.GetEntries()[0].CapabilitySatisfied);
        }

        TEST_CASE_FIXTURE(OverlayFixture, "Overlay deactivates when capabilities become unsatisfied")
        {
            OverlayDescriptor desc;
            desc.RequiredCapabilities.AddTag(Capability::Rendering);
            Stack.AddOverlay(&OverlayA, std::type_index(typeid(TestOverlayA)), desc, 0);

            // Activate with rendering caps.
            CapabilitySet caps;
            caps.AddTag(Capability::Rendering);
            Stack.UpdateCapabilities(caps, Context);
            CHECK(OverlayA.AttachCalled);

            // Remove caps.
            Stack.UpdateCapabilities({}, Context);
            CHECK(OverlayA.DetachCalled);
        }

        TEST_CASE_FIXTURE(OverlayFixture, "Runtime Deactivate sets overlay inactive and calls OnDetach")
        {
            AddDefaultOverlays();
            CHECK(OverlayA.AttachCalled);

            Stack.Deactivate(std::type_index(typeid(TestOverlayA)), Context);
            CHECK(OverlayA.DetachCalled);

            // Should be skipped on update.
            Stack.Update(Context, 0.016f);
            CHECK_FALSE(OverlayA.UpdateCalled);
            CHECK(OverlayB.UpdateCalled);
        }

        TEST_CASE_FIXTURE(OverlayFixture, "Runtime Activate re-enables overlay")
        {
            AddDefaultOverlays();
            Stack.Deactivate(std::type_index(typeid(TestOverlayA)), Context);
            OverlayA.AttachCalled = false;

            Stack.Activate(std::type_index(typeid(TestOverlayA)), Context);
            CHECK(OverlayA.AttachCalled);
            CHECK(Stack.GetEntries()[0].IsActive());
        }

        TEST_CASE_FIXTURE(OverlayFixture, "Priority override sorts overlays correctly")
        {
            OverlayDescriptor descA;
            descA.Priority = 10;
            OverlayDescriptor descB;
            descB.Priority = 5;
            OverlayDescriptor descC;
            descC.Priority = 1;

            Stack.AddOverlay(&OverlayA, std::type_index(typeid(TestOverlayA)), descA, 0);
            Stack.AddOverlay(&OverlayB, std::type_index(typeid(TestOverlayB)), descB, 1);
            Stack.AddOverlay(&OverlayC, std::type_index(typeid(TestOverlayC)), descC, 2);
            Stack.UpdateCapabilities({}, Context);

            // Update order: low to high (C=1, B=5, A=10).
            Stack.Update(Context, 0.016f);
            CHECK(OverlayC.UpdateOrder < OverlayB.UpdateOrder);
            CHECK(OverlayB.UpdateOrder < OverlayA.UpdateOrder);

            // Event order: high to low (A=10, B=5, C=1).
            s_orderCounter = 0;
            Stack.ProcessEvents(Context, Events);
            CHECK(OverlayA.EventOrder < OverlayB.EventOrder);
            CHECK(OverlayB.EventOrder < OverlayC.EventOrder);
        }

        TEST_CASE_FIXTURE(OverlayFixture, "Inactive overlays are skipped during update and render")
        {
            AddDefaultOverlays();
            Stack.Deactivate(std::type_index(typeid(TestOverlayB)), Context);

            Stack.Update(Context, 0.016f);
            CHECK(OverlayA.UpdateCalled);
            CHECK_FALSE(OverlayB.UpdateCalled);
            CHECK(OverlayC.UpdateCalled);

            Stack.Render(Context);
            CHECK(OverlayA.RenderCalled);
            CHECK_FALSE(OverlayB.RenderCalled);
            CHECK(OverlayC.RenderCalled);
        }

        TEST_CASE_FIXTURE(OverlayFixture, "DetachAll detaches all active overlays")
        {
            AddDefaultOverlays();
            Stack.DetachAll(Context);

            CHECK(OverlayA.DetachCalled);
            CHECK(OverlayB.DetachCalled);
            CHECK(OverlayC.DetachCalled);
        }
    }

} // namespace Wayfinder::Tests
