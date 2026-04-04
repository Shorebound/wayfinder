#include "app/AppDescriptor.h"
#include "app/EngineContext.h"
#include "core/InternedString.h"
#include "core/ValidationResult.h"
#include "core/events/EventQueue.h"
#include "plugins/IPlugin.h"
#include "plugins/IStateUI.h"
#include "plugins/PluginConcepts.h"

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

    // -- IPlugin Describe --------------------------------------------------

    class PluginWithDeps : public IPlugin
    {
    public:
        void Build(AppBuilder& builder) override {}

        [[nodiscard]] auto Describe() const -> PluginDescriptor override
        {
            return {
                .Name = InternedString::Intern("TestPlugin"),
                .DependsOn = {std::type_index(typeid(MockPlugin))},
            };
        }
    };

    TEST_SUITE("IPlugin Describe")
    {
        TEST_CASE("Default Describe returns empty descriptor")
        {
            MockPlugin plugin;
            auto desc = plugin.Describe();
            CHECK(desc.Name == "");
            CHECK(desc.DependsOn.empty());
        }

        TEST_CASE("Custom Describe returns specified name and deps")
        {
            PluginWithDeps plugin;
            auto desc = plugin.Describe();
            CHECK(desc.Name == "TestPlugin");
            REQUIRE(desc.DependsOn.size() == 1);
            CHECK(desc.DependsOn[0] == std::type_index(typeid(MockPlugin)));
        }
    }

    // -- PluginConcepts ---------------------------------------------------

    struct TestGroup
    {
        void Build(AppBuilder&) {}
    };

    struct NotAPlugin
    {
        int x = 0;
    };

    static_assert(PluginType<MockPlugin>);
    static_assert(not PluginGroupType<MockPlugin>);
    static_assert(PluginGroupType<TestGroup>);
    static_assert(not PluginType<TestGroup>);
    static_assert(not PluginType<NotAPlugin>);
    static_assert(not PluginGroupType<NotAPlugin>);

    // -- AppDescriptor ----------------------------------------------------

    struct TestOutputA
    {
        int Value = 0;
    };

    struct TestOutputB
    {
        std::string Name;
    };

    TEST_SUITE("AppDescriptor")
    {
        TEST_CASE("Has returns false for absent type")
        {
            AppDescriptor desc;
            CHECK_FALSE(desc.Has<TestOutputA>());
        }

        TEST_CASE("TryGet returns nullptr for absent type")
        {
            AppDescriptor desc;
            CHECK(desc.TryGet<TestOutputA>() == nullptr);
        }

        TEST_CASE("Store and retrieve output")
        {
            AppDescriptor desc;
            desc.AddOutput(TestOutputA{42});

            CHECK(desc.Has<TestOutputA>());
            CHECK(desc.TryGet<TestOutputA>() != nullptr);
            CHECK(desc.Get<TestOutputA>().Value == 42);
        }

        TEST_CASE("Multiple output types stored and retrieved")
        {
            AppDescriptor desc;
            desc.AddOutput(TestOutputA{7});
            desc.AddOutput(TestOutputB{"hello"});

            CHECK(desc.Get<TestOutputA>().Value == 7);
            CHECK(desc.Get<TestOutputB>().Name == "hello");
            CHECK_FALSE(desc.Has<int>());
        }

        TEST_CASE("Move semantics preserve outputs")
        {
            AppDescriptor desc;
            desc.AddOutput(TestOutputA{99});

            AppDescriptor moved = std::move(desc);
            CHECK(moved.Has<TestOutputA>());
            CHECK(moved.Get<TestOutputA>().Value == 99);
        }
    }

    // -- ValidationResult -------------------------------------------------

    TEST_SUITE("ValidationResult")
    {
        TEST_CASE("Fresh result has no errors")
        {
            ValidationResult result;
            CHECK_FALSE(result.HasErrors());
            CHECK(result.ErrorCount() == 0);
            CHECK(result.GetErrors().empty());
        }

        TEST_CASE("AddError accumulates errors")
        {
            ValidationResult result;
            result.AddError("SubsystemRegistrar", "Duplicate registration of FooSubsystem");
            result.AddError("TagRegistrar", "Unknown tag category");

            CHECK(result.HasErrors());
            CHECK(result.ErrorCount() == 2);
            CHECK(result.GetErrors()[0].Source == "SubsystemRegistrar");
            CHECK(result.GetErrors()[0].Message == "Duplicate registration of FooSubsystem");
            CHECK(result.GetErrors()[1].Source == "TagRegistrar");
        }

        TEST_CASE("ToError produces formatted string with source attribution")
        {
            ValidationResult result;
            result.AddError("ConfigRegistrar", "Missing required key 'window.width'");

            auto error = result.ToError();
            auto msg = error.GetMessage();
            CHECK(msg.find("[ConfigRegistrar]") != std::string::npos);
            CHECK(msg.find("Missing required key") != std::string::npos);
        }
    }
}
