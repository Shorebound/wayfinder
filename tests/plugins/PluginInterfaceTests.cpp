#include "app/EngineContext.h"
#include "core/events/EventQueue.h"
#include "plugins/IPlugin.h"
#include "plugins/IStateUI.h"

#include <doctest/doctest.h>

#include <string_view>

using namespace Wayfinder;

namespace Wayfinder::Tests
{
    // -- Mock IPlugin -----------------------------------------------------

    class MockPlugin : public IPlugin
    {
    public:
        bool BuildCalled = false;

        void Build(AppBuilder& builder) override
        {
            BuildCalled = true;
        }
    };

    // -- Mock IStateUI ----------------------------------------------------

    class MockStateUI : public IStateUI
    {
    public:
        bool AttachCalled = false;
        bool DetachCalled = false;
        bool SuspendCalled = false;
        bool ResumeCalled = false;
        bool UpdateCalled = false;
        bool RenderCalled = false;
        bool EventCalled = false;

        auto OnAttach(EngineContext& context) -> Result<void> override
        {
            AttachCalled = true;
            return {};
        }

        auto OnDetach(EngineContext& context) -> Result<void> override
        {
            DetachCalled = true;
            return {};
        }

        void OnSuspend(EngineContext& context) override
        {
            SuspendCalled = true;
        }
        void OnResume(EngineContext& context) override
        {
            ResumeCalled = true;
        }
        void OnUpdate(EngineContext& context, float deltaTime) override
        {
            UpdateCalled = true;
        }
        void OnRender(EngineContext& context) override
        {
            RenderCalled = true;
        }
        void OnEvent(EngineContext& context, EventQueue& events) override
        {
            EventCalled = true;
        }

        [[nodiscard]] auto GetName() const -> std::string_view override
        {
            return "MockStateUI";
        }
    };

    // -- Tests ------------------------------------------------------------

    TEST_SUITE("IPlugin")
    {
        TEST_CASE("Mock compiles and instantiates")
        {
            MockPlugin plugin;
            CHECK_FALSE(plugin.BuildCalled);
            // Build requires an AppBuilder which doesn't exist yet (Phase 3).
            // Compilation of the mock proves the interface is derivable.
        }
    }

    TEST_SUITE("IStateUI")
    {
        TEST_CASE("Mock compiles and implements lifecycle")
        {
            MockStateUI stateUI;
            CHECK(stateUI.GetName() == "MockStateUI");
            CHECK_FALSE(stateUI.AttachCalled);
            CHECK_FALSE(stateUI.DetachCalled);
            CHECK_FALSE(stateUI.SuspendCalled);
            CHECK_FALSE(stateUI.ResumeCalled);
        }
    }
}
