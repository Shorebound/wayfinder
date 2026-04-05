#include "app/AppBuilder.h"
#include "app/AppDescriptor.h"
#include "app/AppSubsystem.h"
#include "app/IApplicationState.h"
#include "app/IOverlay.h"
#include "app/OverlayManifest.h"
#include "app/StateManifest.h"
#include "app/SubsystemRegistry.h"
#include "core/InternedString.h"
#include "core/ValidationResult.h"
#include "gameplay/Capability.h"
#include "gameplay/NativeTag.h"
#include "plugins/IPlugin.h"
#include "plugins/IStateUI.h"
#include "plugins/LifecycleHooks.h"
#include "plugins/PluginConcepts.h"
#include "plugins/PluginDescriptor.h"
#include "plugins/registrars/StateRegistrar.h"
#include "plugins/registrars/TagRegistrar.h"

#include <doctest/doctest.h>

using namespace Wayfinder;

namespace Wayfinder::Tests
{
    // -- Test helpers ---------------------------------------------------------

    class EmptyPlugin : public IPlugin
    {
    public:
        bool Built = false;
        void Build(AppBuilder& builder) override
        {
            Built = true;
        }
    };

    static int s_buildCounter = 0;

    class PluginA : public IPlugin
    {
    public:
        int MyOrder = -1;

        auto Describe() const -> PluginDescriptor override
        {
            return {.Name = InternedString::Intern("PluginA")};
        }
        void Build(AppBuilder& builder) override
        {
            MyOrder = s_buildCounter++;
        }
    };

    class PluginB : public IPlugin
    {
    public:
        int MyOrder = -1;

        auto Describe() const -> PluginDescriptor override
        {
            return {
                .Name = InternedString::Intern("PluginB"),
                .DependsOn = {std::type_index(typeid(PluginA))},
            };
        }
        void Build(AppBuilder& builder) override
        {
            MyOrder = s_buildCounter++;
        }
    };

    class PluginC : public IPlugin
    {
    public:
        int MyOrder = -1;

        auto Describe() const -> PluginDescriptor override
        {
            return {
                .Name = InternedString::Intern("PluginC"),
                .DependsOn = {std::type_index(typeid(PluginB))},
            };
        }
        void Build(AppBuilder& builder) override
        {
            MyOrder = s_buildCounter++;
        }
    };

    class CyclePluginB;

    class CyclePluginA : public IPlugin
    {
    public:
        auto Describe() const -> PluginDescriptor override;
        void Build(AppBuilder& builder) override {}
    };

    class CyclePluginB : public IPlugin
    {
    public:
        auto Describe() const -> PluginDescriptor override
        {
            return {
                .Name = InternedString::Intern("CycleB"),
                .DependsOn = {std::type_index(typeid(CyclePluginA))},
            };
        }
        void Build(AppBuilder& builder) override {}
    };

    auto CyclePluginA::Describe() const -> PluginDescriptor
    {
        return {
            .Name = InternedString::Intern("CycleA"),
            .DependsOn = {std::type_index(typeid(CyclePluginB))},
        };
    }

    class MissingDepPlugin : public IPlugin
    {
    public:
        auto Describe() const -> PluginDescriptor override
        {
            return {
                .Name = InternedString::Intern("MissingDep"),
                .DependsOn = {std::type_index(typeid(int))}, // non-existent
            };
        }
        void Build(AppBuilder& builder) override {}
    };

    struct TestPluginGroup
    {
        bool Built = false;
        void Build(AppBuilder& builder)
        {
            builder.AddPlugin<PluginA>();
            builder.AddPlugin<EmptyPlugin>();
            Built = true;
        }
    };

    static_assert(PluginGroupType<TestPluginGroup>);
    static_assert(not PluginType<TestPluginGroup>);

    // -- Test suites ----------------------------------------------------------

    TEST_SUITE("AppBuilder Registration")
    {
        TEST_CASE("Empty builder finalises successfully")
        {
            AppBuilder builder;
            auto result = builder.Finalise();
            CHECK(result.has_value());
        }

        TEST_CASE("Single plugin registers and builds")
        {
            AppBuilder builder;
            builder.AddPlugin<EmptyPlugin>();
            auto result = builder.Finalise();
            CHECK(result.has_value());
        }

        TEST_CASE("Multiple plugins register successfully")
        {
            s_buildCounter = 0;
            AppBuilder builder;
            builder.AddPlugin<PluginA>();
            builder.AddPlugin<EmptyPlugin>();
            auto result = builder.Finalise();
            CHECK(result.has_value());
        }

        TEST_CASE("Duplicate plugin silently skipped")
        {
            s_buildCounter = 0;
            AppBuilder builder;
            builder.AddPlugin<PluginA>();
            builder.AddPlugin<PluginA>(); // duplicate
            auto result = builder.Finalise();
            REQUIRE(result.has_value());
            // Build counter should only be 1 (not 2)
            CHECK(s_buildCounter == 1);
        }
    }

    TEST_SUITE("AppBuilder Dependency Ordering")
    {
        TEST_CASE("Dependent plugin builds after dependency")
        {
            s_buildCounter = 0;
            AppBuilder builder;
            // Register B first, then A - should still build A before B
            builder.AddPlugin<PluginB>();
            builder.AddPlugin<PluginA>();
            auto result = builder.Finalise();
            REQUIRE(result.has_value());
            // PluginA should have a lower order than PluginB
            CHECK(s_buildCounter == 2);
        }

        TEST_CASE("Three-plugin chain builds in correct order")
        {
            s_buildCounter = 0;
            AppBuilder builder;
            builder.AddPlugin<PluginC>();
            builder.AddPlugin<PluginA>();
            builder.AddPlugin<PluginB>();
            auto result = builder.Finalise();
            REQUIRE(result.has_value());
            CHECK(s_buildCounter == 3);
        }

        TEST_CASE("Missing dependency detected at Finalise")
        {
            AppBuilder builder;
            builder.AddPlugin<MissingDepPlugin>();
            auto result = builder.Finalise();
            CHECK_FALSE(result.has_value());
            CHECK(result.error().GetMessage().find("unregistered") != std::string::npos);
        }

        TEST_CASE("Cycle detected at Finalise")
        {
            AppBuilder builder;
            builder.AddPlugin<CyclePluginA>();
            builder.AddPlugin<CyclePluginB>();
            auto result = builder.Finalise();
            CHECK_FALSE(result.has_value());
            CHECK(result.error().GetMessage().find("cycle") != std::string::npos);
        }
    }

    TEST_SUITE("AppBuilder Plugin Groups")
    {
        TEST_CASE("Group expands into component plugins")
        {
            s_buildCounter = 0;
            AppBuilder builder;
            builder.AddPlugin<TestPluginGroup>();
            auto result = builder.Finalise();
            REQUIRE(result.has_value());
            // Both PluginA and EmptyPlugin should have been built
            CHECK(s_buildCounter >= 1);
        }

        TEST_CASE("Duplicate from group expansion silently skipped")
        {
            s_buildCounter = 0;
            AppBuilder builder;
            builder.AddPlugin<PluginA>();
            builder.AddPlugin<TestPluginGroup>(); // group also adds PluginA
            auto result = builder.Finalise();
            REQUIRE(result.has_value());
        }
    }

    TEST_SUITE("AppBuilder Lifecycle Hooks")
    {
        TEST_CASE("OnAppReady hook stored in manifest")
        {
            AppBuilder builder;
            bool called = false;
            builder.OnAppReady([&](EngineContext&)
            {
                called = true;
            });
            auto result = builder.Finalise();
            REQUIRE(result.has_value());

            const auto* manifest = result->TryGet<LifecycleHookManifest>();
            REQUIRE(manifest != nullptr);
            CHECK(manifest->OnAppReady.size() == 1);
        }

        TEST_CASE("OnShutdown hook stored in manifest")
        {
            AppBuilder builder;
            builder.OnShutdown([]
            {
            });
            auto result = builder.Finalise();
            REQUIRE(result.has_value());

            const auto* manifest = result->TryGet<LifecycleHookManifest>();
            REQUIRE(manifest != nullptr);
            CHECK(manifest->OnShutdown.size() == 1);
        }

        TEST_CASE("OnStateEnter hook stored under correct type")
        {
            struct TestState {};

            AppBuilder builder;
            builder.OnStateEnter<TestState>([](EngineContext&)
            {
            });
            auto result = builder.Finalise();
            REQUIRE(result.has_value());

            const auto* manifest = result->TryGet<LifecycleHookManifest>();
            REQUIRE(manifest != nullptr);
            CHECK(manifest->OnStateEnter.contains(std::type_index(typeid(TestState))));
        }

        TEST_CASE("Multiple hooks of same type stored in order")
        {
            AppBuilder builder;
            int order = 0;
            int first = -1;
            int second = -1;
            builder.OnAppReady([&](EngineContext&)
            {
                first = order++;
            });
            builder.OnAppReady([&](EngineContext&)
            {
                second = order++;
            });
            auto result = builder.Finalise();
            REQUIRE(result.has_value());

            const auto* manifest = result->TryGet<LifecycleHookManifest>();
            REQUIRE(manifest != nullptr);
            CHECK(manifest->OnAppReady.size() == 2);
        }
    }

    TEST_SUITE("AppBuilder Registrar Store")
    {
        TEST_CASE("Registrar created on first access")
        {
            AppBuilder builder;

            using namespace Wayfinder::Plugins;
            auto& tagReg = builder.GetRegistrar<TagRegistrar>();
            tagReg.Register({.Name = "Test.Tag", .Comment = "A test tag"});
            CHECK(tagReg.GetDescriptors().size() == 1);
        }

        TEST_CASE("Second access returns same instance")
        {
            AppBuilder builder;

            using namespace Wayfinder::Plugins;
            auto& first = builder.GetRegistrar<TagRegistrar>();
            auto& second = builder.GetRegistrar<TagRegistrar>();
            CHECK(&first == &second);
        }

        TEST_CASE("Multiple registrar types coexist")
        {
            AppBuilder builder;

            using namespace Wayfinder::Plugins;
            auto& tags = builder.GetRegistrar<TagRegistrar>();
            auto& states = builder.GetRegistrar<StateRegistrar>();
            tags.Register({.Name = "Tag.A"});
            states.Register({.Name = "MainMenu"});
            states.SetInitial("MainMenu");
            CHECK(tags.GetDescriptors().size() == 1);
            CHECK(states.GetInitial() == "MainMenu");
        }

        TEST_CASE("TakeRegistrar extracts and removes from store")
        {
            AppBuilder builder;

            using namespace Wayfinder::Plugins;
            builder.GetRegistrar<TagRegistrar>().Register({.Name = "Tag.X"});

            auto taken = builder.TakeRegistrar<TagRegistrar>();
            REQUIRE(taken != nullptr);
            CHECK(taken->GetDescriptors().size() == 1);

            // Second take returns nullptr
            auto second = builder.TakeRegistrar<TagRegistrar>();
            CHECK(second == nullptr);
        }
    }

    TEST_SUITE("AppBuilder Finalise")
    {
        TEST_CASE("Finalise returns AppDescriptor on success")
        {
            AppBuilder builder;
            builder.AddPlugin<EmptyPlugin>();
            auto result = builder.Finalise();
            REQUIRE(result.has_value());
        }

        TEST_CASE("AppDescriptor contains LifecycleHookManifest when hooks registered")
        {
            AppBuilder builder;
            builder.OnShutdown([]
            {
            });
            auto result = builder.Finalise();
            REQUIRE(result.has_value());
            CHECK(result->Has<LifecycleHookManifest>());
        }

        TEST_CASE("AppDescriptor has no LifecycleHookManifest when no hooks registered")
        {
            AppBuilder builder;
            auto result = builder.Finalise();
            REQUIRE(result.has_value());
            CHECK_FALSE(result->Has<LifecycleHookManifest>());
        }
    }

    // -- State / overlay / UI mock types ------------------------------------

    class MockStateA : public IApplicationState
    {
    public:
        [[nodiscard]] auto OnEnter(EngineContext& /*context*/) -> Result<void> override
        {
            return {};
        }
        [[nodiscard]] auto OnExit(EngineContext& /*context*/) -> Result<void> override
        {
            return {};
        }
        [[nodiscard]] auto GetName() const -> std::string_view override
        {
            return "MockStateA";
        }
    };

    class MockStateB : public IApplicationState
    {
    public:
        [[nodiscard]] auto OnEnter(EngineContext& /*context*/) -> Result<void> override
        {
            return {};
        }
        [[nodiscard]] auto OnExit(EngineContext& /*context*/) -> Result<void> override
        {
            return {};
        }
        [[nodiscard]] auto GetName() const -> std::string_view override
        {
            return "MockStateB";
        }
    };

    class MockOverlayA : public IOverlay
    {
    public:
        [[nodiscard]] auto OnAttach(EngineContext& /*context*/) -> Result<void> override
        {
            return {};
        }
        [[nodiscard]] auto OnDetach(EngineContext& /*context*/) -> Result<void> override
        {
            return {};
        }
        [[nodiscard]] auto GetName() const -> std::string_view override
        {
            return "MockOverlayA";
        }
    };

    class MockStateUIForA : public IStateUI
    {
    public:
        [[nodiscard]] auto OnAttach(EngineContext& /*context*/) -> Result<void> override
        {
            return {};
        }
        [[nodiscard]] auto OnDetach(EngineContext& /*context*/) -> Result<void> override
        {
            return {};
        }
        [[nodiscard]] auto GetName() const -> std::string_view override
        {
            return "MockStateUIForA";
        }
    };

    TEST_SUITE("AppBuilder State and Overlay Registration")
    {
        TEST_CASE("AddState registers state and Finalise produces StateManifest")
        {
            AppBuilder builder;
            builder.AddState<MockStateA>();
            builder.SetInitialState<MockStateA>();
            auto result = builder.Finalise();
            REQUIRE(result.has_value());
            CHECK(result->Has<StateManifest>());

            const auto& manifest = result->Get<StateManifest>();
            CHECK(manifest.States.size() == 1);
            CHECK(manifest.InitialState == std::type_index(typeid(MockStateA)));
        }

        TEST_CASE("AddTransition stores flat transition in StateManifest")
        {
            AppBuilder builder;
            builder.AddState<MockStateA>();
            builder.AddState<MockStateB>();
            builder.SetInitialState<MockStateA>();
            builder.AddTransition<MockStateA, MockStateB>();
            auto result = builder.Finalise();
            REQUIRE(result.has_value());

            const auto& manifest = result->Get<StateManifest>();
            auto it = manifest.FlatTransitions.find(std::type_index(typeid(MockStateA)));
            REQUIRE(it != manifest.FlatTransitions.end());
            CHECK(it->second.contains(std::type_index(typeid(MockStateB))));
        }

        TEST_CASE("AllowPush stores pushable state in StateManifest")
        {
            AppBuilder builder;
            builder.AddState<MockStateA>();
            builder.AddState<MockStateB>();
            builder.SetInitialState<MockStateA>();
            builder.AllowPush<MockStateB>();
            auto result = builder.Finalise();
            REQUIRE(result.has_value());

            const auto& manifest = result->Get<StateManifest>();
            CHECK(manifest.PushableStates.contains(std::type_index(typeid(MockStateB))));
        }

        TEST_CASE("RegisterOverlay produces OverlayManifest")
        {
            AppBuilder builder;
            builder.RegisterOverlay<MockOverlayA>();
            auto result = builder.Finalise();
            REQUIRE(result.has_value());
            CHECK(result->Has<OverlayManifest>());

            const auto& manifest = result->Get<OverlayManifest>();
            CHECK(manifest.Overlays.size() == 1);
            CHECK(manifest.Overlays[0].Type == std::type_index(typeid(MockOverlayA)));
        }

        TEST_CASE("ForState<T>().SetUI<U>() stores UI factory in StateManifest")
        {
            AppBuilder builder;
            builder.AddState<MockStateA>();
            builder.SetInitialState<MockStateA>();
            builder.ForState<MockStateA>().SetUI<MockStateUIForA>();
            auto result = builder.Finalise();
            REQUIRE(result.has_value());

            const auto& manifest = result->Get<StateManifest>();
            auto it = manifest.StateUIFactories.find(std::type_index(typeid(MockStateA)));
            REQUIRE(it != manifest.StateUIFactories.end());
            auto ui = it->second();
            REQUIRE(ui != nullptr);
            CHECK(ui->GetName() == "MockStateUIForA");
        }

        TEST_CASE("State factory produces correct type")
        {
            AppBuilder builder;
            builder.AddState<MockStateA>();
            builder.SetInitialState<MockStateA>();
            auto result = builder.Finalise();
            REQUIRE(result.has_value());

            const auto& manifest = result->Get<StateManifest>();
            REQUIRE(manifest.States.size() == 1);
            auto state = manifest.States[0].Factory();
            REQUIRE(state != nullptr);
            CHECK(dynamic_cast<MockStateA*>(state.get()) != nullptr);
        }

        TEST_CASE("Overlay factory produces correct type")
        {
            AppBuilder builder;
            builder.RegisterOverlay<MockOverlayA>();
            auto result = builder.Finalise();
            REQUIRE(result.has_value());

            const auto& manifest = result->Get<OverlayManifest>();
            REQUIRE(manifest.Overlays.size() == 1);
            auto overlay = manifest.Overlays[0].Factory();
            REQUIRE(overlay != nullptr);
            CHECK(dynamic_cast<MockOverlayA*>(overlay.get()) != nullptr);
        }

        TEST_CASE("Empty state registration produces no StateManifest")
        {
            AppBuilder builder;
            auto result = builder.Finalise();
            REQUIRE(result.has_value());
            CHECK_FALSE(result->Has<StateManifest>());
        }

        TEST_CASE("Empty overlay registration produces no OverlayManifest")
        {
            AppBuilder builder;
            auto result = builder.Finalise();
            REQUIRE(result.has_value());
            CHECK_FALSE(result->Has<OverlayManifest>());
        }

        TEST_CASE("State capabilities stored correctly")
        {
            TagRegistry registry;
            NativeTag::RegisterAll(registry);

            CapabilitySet capabilities;
            capabilities.AddTag(Capability::Simulation);

            AppBuilder builder;
            builder.AddState<MockStateA>(std::move(capabilities));
            builder.SetInitialState<MockStateA>();
            auto result = builder.Finalise();
            REQUIRE(result.has_value());

            const auto& manifest = result->Get<StateManifest>();
            REQUIRE(manifest.States.size() == 1);
            CHECK_FALSE(manifest.States[0].Capabilities.IsEmpty());
        }

        TEST_CASE("Overlay descriptor preserved through Finalise")
        {
            AppBuilder builder;
            builder.RegisterOverlay<MockOverlayA>(OverlayDescriptor{
                .Priority = 42,
                .DefaultActive = false,
            });
            auto result = builder.Finalise();
            REQUIRE(result.has_value());

            const auto& manifest = result->Get<OverlayManifest>();
            REQUIRE(manifest.Overlays.size() == 1);
            CHECK(manifest.Overlays[0].Descriptor.Priority == 42);
            CHECK_FALSE(manifest.Overlays[0].Descriptor.DefaultActive);
        }
    }
}
