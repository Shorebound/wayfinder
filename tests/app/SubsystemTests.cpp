#include "app/AppSubsystem.h"
#include "app/EngineConfig.h"
#include "app/EngineContext.h"
#include "app/EngineRuntime.h"
#include "app/StateSubsystem.h"
#include "app/Subsystem.h"
#include "app/SubsystemRegistry.h"
#include "gameplay/Capability.h"
#include "gameplay/NativeTag.h"
#include "gameplay/TagRegistry.h"
#include "platform/BackendConfig.h"
#include "project/ProjectDescriptor.h"

#include <doctest/doctest.h>

#include <string>
#include <typeindex>
#include <vector>

namespace Wayfinder::Tests
{
    /// Tracks lifecycle calls for testing.
    struct LifecycleLog
    {
        std::vector<std::string> Events;
    };

    static LifecycleLog* s_log = nullptr;

    class SubsystemA : public GameSubsystem
    {
    public:
        void Initialise() override
        {
            if (s_log)
            {
                s_log->Events.push_back("A.Init");
            }
        }
        void Shutdown() override
        {
            if (s_log)
            {
                s_log->Events.push_back("A.Shutdown");
            }
        }
    };

    class SubsystemB : public GameSubsystem
    {
    public:
        void Initialise() override
        {
            if (s_log)
            {
                s_log->Events.push_back("B.Init");
            }
        }
        void Shutdown() override
        {
            if (s_log)
            {
                s_log->Events.push_back("B.Shutdown");
            }
        }
    };

    class SubsystemC : public GameSubsystem
    {
    public:
        bool ShouldCreate() const override
        {
            return false;
        }
        void Initialise() override
        {
            if (s_log)
            {
                s_log->Events.push_back("C.Init");
            }
        }
        void Shutdown() override
        {
            if (s_log)
            {
                s_log->Events.push_back("C.Shutdown");
            }
        }
    };

    TEST_SUITE("SubsystemCollection")
    {
        TEST_CASE("Register and Initialise creates subsystems")
        {
            SubsystemCollection<GameSubsystem> collection;
            collection.Register<SubsystemA>();
            collection.Register<SubsystemB>();

            collection.Initialise();

            CHECK(collection.Get<SubsystemA>() != nullptr);
            CHECK(collection.Get<SubsystemB>() != nullptr);

            collection.Shutdown();
        }

        TEST_CASE("Get returns nullptr for unregistered subsystem")
        {
            SubsystemCollection<GameSubsystem> collection;
            collection.Initialise();

            CHECK(collection.Get<SubsystemA>() == nullptr);

            collection.Shutdown();
        }

        TEST_CASE("Duplicate registration is rejected")
        {
            SubsystemCollection<GameSubsystem> collection;
            CHECK(collection.Register<SubsystemA>() == true);
            CHECK(collection.Register<SubsystemA>() == false);

            collection.Shutdown();
        }

        TEST_CASE("Initialise calls Initialise on each subsystem")
        {
            LifecycleLog log;
            s_log = &log;

            SubsystemCollection<GameSubsystem> collection;
            collection.Register<SubsystemA>();
            collection.Register<SubsystemB>();
            collection.Initialise();

            REQUIRE(log.Events.size() == 2);
            CHECK(log.Events[0] == "A.Init");
            CHECK(log.Events[1] == "B.Init");

            collection.Shutdown();
            s_log = nullptr;
        }

        TEST_CASE("Shutdown calls Shutdown in reverse order")
        {
            LifecycleLog log;
            s_log = &log;

            SubsystemCollection<GameSubsystem> collection;
            collection.Register<SubsystemA>();
            collection.Register<SubsystemB>();
            collection.Initialise();

            log.Events.clear();
            collection.Shutdown();

            REQUIRE(log.Events.size() == 2);
            CHECK(log.Events[0] == "B.Shutdown");
            CHECK(log.Events[1] == "A.Shutdown");

            s_log = nullptr;
        }

        TEST_CASE("ShouldCreate=false skips subsystem creation")
        {
            LifecycleLog log;
            s_log = &log;

            SubsystemCollection<GameSubsystem> collection;
            collection.Register<SubsystemC>(); // ShouldCreate returns false
            collection.Initialise();

            CHECK(collection.Get<SubsystemC>() == nullptr);
            CHECK(log.Events.empty()); // Init never called

            collection.Shutdown();
            s_log = nullptr;
        }

        TEST_CASE("Static predicate gating skips creation")
        {
            LifecycleLog log;
            s_log = &log;

            SubsystemCollection<GameSubsystem> collection;
            collection.Register<SubsystemA>([]() -> bool
            {
                return false;
            });
            collection.Initialise();

            CHECK(collection.Get<SubsystemA>() == nullptr);
            CHECK(log.Events.empty());

            collection.Shutdown();
            s_log = nullptr;
        }

        TEST_CASE("Subsystems are gone after Shutdown")
        {
            SubsystemCollection<GameSubsystem> collection;
            collection.Register<SubsystemA>();
            collection.Initialise();

            REQUIRE(collection.Get<SubsystemA>() != nullptr);

            collection.Shutdown();

            CHECK(collection.Get<SubsystemA>() == nullptr);
        }

        TEST_CASE("Const Get returns const pointer")
        {
            SubsystemCollection<GameSubsystem> collection;
            collection.Register<SubsystemA>();
            collection.Initialise();

            const auto& constCollection = collection;
            const SubsystemA* ptr = constCollection.Get<SubsystemA>();
            CHECK(ptr != nullptr);

            collection.Shutdown();
        }
    }

    // -- AppSubsystem / StateSubsystem scoping ----------------------------

    class TestAppSubsystem : public AppSubsystem
    {
    public:
        void Initialise() override
        {
            if (s_log)
            {
                s_log->Events.push_back("App.Init");
            }
        }
        void Shutdown() override
        {
            if (s_log)
            {
                s_log->Events.push_back("App.Shutdown");
            }
        }
    };

    class TestStateSubsystem : public StateSubsystem
    {
    public:
        void Initialise() override
        {
            if (s_log)
            {
                s_log->Events.push_back("State.Init");
            }
        }
        void Shutdown() override
        {
            if (s_log)
            {
                s_log->Events.push_back("State.Shutdown");
            }
        }
    };

    TEST_SUITE("AppSubsystem")
    {
        TEST_CASE("Registers and initialises in SubsystemCollection")
        {
            LifecycleLog log;
            s_log = &log;

            SubsystemCollection<AppSubsystem> collection;
            collection.Register<TestAppSubsystem>();
            collection.Initialise();

            CHECK(collection.Get<TestAppSubsystem>() != nullptr);
            REQUIRE(log.Events.size() == 1);
            CHECK(log.Events[0] == "App.Init");

            collection.Shutdown();
            REQUIRE(log.Events.size() == 2);
            CHECK(log.Events[1] == "App.Shutdown");

            s_log = nullptr;
        }
    }

    TEST_SUITE("StateSubsystem")
    {
        TEST_CASE("Registers and initialises in SubsystemCollection")
        {
            LifecycleLog log;
            s_log = &log;

            SubsystemCollection<StateSubsystem> collection;
            collection.Register<TestStateSubsystem>();
            collection.Initialise();

            CHECK(collection.Get<TestStateSubsystem>() != nullptr);
            REQUIRE(log.Events.size() == 1);
            CHECK(log.Events[0] == "State.Init");

            collection.Shutdown();
            REQUIRE(log.Events.size() == 2);
            CHECK(log.Events[1] == "State.Shutdown");

            s_log = nullptr;
        }
    }

    // -- SubsystemRegistry test fixtures ----------------------------------

    /// RAII wrapper that provides a valid EngineContext for SubsystemRegistry tests.
    struct TestEngineRuntime
    {
        EngineConfig Config;
        ProjectDescriptor Project;
        std::unique_ptr<EngineRuntime> Runtime;

        TestEngineRuntime()
        {
            Config.Backends.Platform = PlatformBackend::Null;
            Config.Backends.Rendering = RenderBackend::Null;
            Config.Window = {.Width = 320, .Height = 240, .Title = "Test"};
            Project.Name = "SubsystemRegistryTest";
            Runtime = std::make_unique<EngineRuntime>(Config, Project);
            Runtime->Initialise();
        }

        ~TestEngineRuntime()
        {
            Runtime->Shutdown();
        }

        auto GetContext() -> EngineContext
        {
            return EngineContext{};
        }
    };

    /// Ensures NativeTag constants are registered for capability tests.
    struct TagRegistryFixture
    {
        TagRegistry Registry;

        TagRegistryFixture()
        {
            NativeTag::RegisterAll(Registry);
        }
    };

    // -- Registry test subsystems -----------------------------------------

    class RegSubA : public AppSubsystem
    {
    public:
        auto Initialise(EngineContext& context) -> Result<void> override
        {
            if (s_log)
            {
                s_log->Events.push_back("A.Init");
            }
            return {};
        }
        void Shutdown() override
        {
            if (s_log)
            {
                s_log->Events.push_back("A.Shutdown");
            }
        }
    };

    class RegSubB : public AppSubsystem
    {
    public:
        auto Initialise(EngineContext& context) -> Result<void> override
        {
            if (s_log)
            {
                s_log->Events.push_back("B.Init");
            }
            return {};
        }
        void Shutdown() override
        {
            if (s_log)
            {
                s_log->Events.push_back("B.Shutdown");
            }
        }
    };

    class RegSubC : public AppSubsystem
    {
    public:
        auto Initialise(EngineContext& context) -> Result<void> override
        {
            if (s_log)
            {
                s_log->Events.push_back("C.Init");
            }
            return {};
        }
        void Shutdown() override
        {
            if (s_log)
            {
                s_log->Events.push_back("C.Shutdown");
            }
        }
    };

    class FailingSub : public AppSubsystem
    {
    public:
        auto Initialise(EngineContext& context) -> Result<void> override
        {
            if (s_log)
            {
                s_log->Events.push_back("Fail.Init");
            }
            return MakeError("FailingSub init failed");
        }
        void Shutdown() override
        {
            if (s_log)
            {
                s_log->Events.push_back("Fail.Shutdown");
            }
        }
    };

    class IAbstractService : public AppSubsystem
    {
    public:
        virtual auto GetValue() const -> int = 0;
    };

    class ConcreteService : public IAbstractService
    {
    public:
        auto Initialise(EngineContext& context) -> Result<void> override
        {
            return {};
        }
        auto GetValue() const -> int override
        {
            return 42;
        }
    };

    class CapGatedSub : public AppSubsystem
    {
    public:
        auto Initialise(EngineContext& context) -> Result<void> override
        {
            if (s_log)
            {
                s_log->Events.push_back("CapGated.Init");
            }
            return {};
        }
        void Shutdown() override
        {
            if (s_log)
            {
                s_log->Events.push_back("CapGated.Shutdown");
            }
        }
    };

    TEST_SUITE("SubsystemRegistry")
    {
        TEST_CASE("Register and Initialise creates subsystems")
        {
            TestEngineRuntime runtime;
            auto ctx = runtime.GetContext();

            SubsystemRegistry<AppSubsystem> registry;
            registry.Register<RegSubA>();
            registry.Register<RegSubB>();

            REQUIRE(registry.Finalise());
            REQUIRE(registry.Initialise(ctx, CapabilitySet{}));

            CHECK(registry.TryGet<RegSubA>() != nullptr);
            CHECK(registry.TryGet<RegSubB>() != nullptr);

            registry.Shutdown();
        }

        TEST_CASE("Topological init order respected")
        {
            LifecycleLog log;
            s_log = &log;

            TestEngineRuntime runtime;
            auto ctx = runtime.GetContext();

            SubsystemRegistry<AppSubsystem> registry;
            // Register B first, with dependency on A
            registry.Register<RegSubB>({.DependsOn = Deps<RegSubA>()});
            registry.Register<RegSubA>();

            REQUIRE(registry.Finalise());
            REQUIRE(registry.Initialise(ctx, CapabilitySet{}));

            // A must initialise before B despite B being registered first
            REQUIRE(log.Events.size() >= 2);
            CHECK(log.Events[0] == "A.Init");
            CHECK(log.Events[1] == "B.Init");

            registry.Shutdown();
            s_log = nullptr;
        }

        TEST_CASE("Reverse shutdown order")
        {
            LifecycleLog log;
            s_log = &log;

            TestEngineRuntime runtime;
            auto ctx = runtime.GetContext();

            SubsystemRegistry<AppSubsystem> registry;
            registry.Register<RegSubB>({.DependsOn = Deps<RegSubA>()});
            registry.Register<RegSubA>();

            REQUIRE(registry.Finalise());
            REQUIRE(registry.Initialise(ctx, CapabilitySet{}));

            log.Events.clear();
            registry.Shutdown();

            // B shuts down before A (reverse topological)
            REQUIRE(log.Events.size() == 2);
            CHECK(log.Events[0] == "B.Shutdown");
            CHECK(log.Events[1] == "A.Shutdown");

            s_log = nullptr;
        }

        TEST_CASE("Three-node dependency chain")
        {
            LifecycleLog log;
            s_log = &log;

            TestEngineRuntime runtime;
            auto ctx = runtime.GetContext();

            SubsystemRegistry<AppSubsystem> registry;
            registry.Register<RegSubC>({.DependsOn = Deps<RegSubB>()});
            registry.Register<RegSubA>();
            registry.Register<RegSubB>({.DependsOn = Deps<RegSubA>()});

            REQUIRE(registry.Finalise());
            REQUIRE(registry.Initialise(ctx, CapabilitySet{}));

            // Init: A, B, C
            REQUIRE(log.Events.size() >= 3);
            CHECK(log.Events[0] == "A.Init");
            CHECK(log.Events[1] == "B.Init");
            CHECK(log.Events[2] == "C.Init");

            log.Events.clear();
            registry.Shutdown();

            // Shutdown: C, B, A
            REQUIRE(log.Events.size() == 3);
            CHECK(log.Events[0] == "C.Shutdown");
            CHECK(log.Events[1] == "B.Shutdown");
            CHECK(log.Events[2] == "A.Shutdown");

            s_log = nullptr;
        }

        TEST_CASE("Cycle detection with two nodes")
        {
            SubsystemRegistry<AppSubsystem> registry;
            registry.Register<RegSubA>({.DependsOn = Deps<RegSubB>()});
            registry.Register<RegSubB>({.DependsOn = Deps<RegSubA>()});

            auto result = registry.Finalise();
            CHECK(not result.has_value());
            CHECK(result.error().GetMessage().find("Cycle detected:") != std::string::npos);
        }

        TEST_CASE("Cycle detection with three nodes")
        {
            SubsystemRegistry<AppSubsystem> registry;
            registry.Register<RegSubA>({.DependsOn = Deps<RegSubC>()});
            registry.Register<RegSubB>({.DependsOn = Deps<RegSubA>()});
            registry.Register<RegSubC>({.DependsOn = Deps<RegSubB>()});

            auto result = registry.Finalise();
            CHECK(not result.has_value());
            CHECK(result.error().GetMessage().find("Cycle detected:") != std::string::npos);
        }

        TEST_CASE("Unregistered dependency detected at Finalise")
        {
            SubsystemRegistry<AppSubsystem> registry;
            registry.Register<RegSubA>({.DependsOn = Deps<RegSubB>()});
            // B is never registered

            auto result = registry.Finalise();
            CHECK(not result.has_value());
            CHECK(result.error().GetMessage().find("unregistered") != std::string::npos);
        }

        TEST_CASE("Abstract-type resolution via Get")
        {
            TestEngineRuntime runtime;
            auto ctx = runtime.GetContext();

            SubsystemRegistry<AppSubsystem> registry;
            registry.Register<ConcreteService, IAbstractService>();

            REQUIRE(registry.Finalise());
            REQUIRE(registry.Initialise(ctx, CapabilitySet{}));

            // Query by abstract type
            auto* abstractPtr = registry.TryGet<IAbstractService>();
            REQUIRE(abstractPtr != nullptr);
            CHECK(abstractPtr->GetValue() == 42);

            // Query by concrete type - same instance
            auto* concretePtr = registry.TryGet<ConcreteService>();
            REQUIRE(concretePtr != nullptr);
            CHECK(static_cast<const void*>(abstractPtr) == static_cast<const void*>(concretePtr));

            registry.Shutdown();
        }

        TEST_CASE("Capability gating skips subsystem when not satisfied")
        {
            LifecycleLog log;
            s_log = &log;
            TagRegistryFixture tags;

            TestEngineRuntime runtime;
            auto ctx = runtime.GetContext();

            CapabilitySet required;
            required.AddTag(Capability::Rendering);

            SubsystemRegistry<AppSubsystem> registry;
            registry.Register<CapGatedSub>({.RequiredCapabilities = required});

            REQUIRE(registry.Finalise());

            // Initialise with only Simulation - Rendering not present
            CapabilitySet effective;
            effective.AddTag(Capability::Simulation);
            REQUIRE(registry.Initialise(ctx, effective));

            CHECK(registry.TryGet<CapGatedSub>() == nullptr);
            CHECK(log.Events.empty()); // Never initialised

            registry.Shutdown();
            s_log = nullptr;
        }

        TEST_CASE("Capability gating activates when satisfied")
        {
            LifecycleLog log;
            s_log = &log;
            TagRegistryFixture tags;

            TestEngineRuntime runtime;
            auto ctx = runtime.GetContext();

            CapabilitySet required;
            required.AddTag(Capability::Rendering);

            SubsystemRegistry<AppSubsystem> registry;
            registry.Register<CapGatedSub>({.RequiredCapabilities = required});

            REQUIRE(registry.Finalise());

            // Initialise with Rendering present
            CapabilitySet effective;
            effective.AddTag(Capability::Rendering);
            effective.AddTag(Capability::Simulation);
            REQUIRE(registry.Initialise(ctx, effective));

            CHECK(registry.TryGet<CapGatedSub>() != nullptr);
            REQUIRE(log.Events.size() == 1);
            CHECK(log.Events[0] == "CapGated.Init");

            registry.Shutdown();
            s_log = nullptr;
        }

        TEST_CASE("Empty RequiredCapabilities always activates")
        {
            TestEngineRuntime runtime;
            auto ctx = runtime.GetContext();

            SubsystemRegistry<AppSubsystem> registry;
            registry.Register<RegSubA>(); // Default descriptor - empty caps

            REQUIRE(registry.Finalise());
            REQUIRE(registry.Initialise(ctx, CapabilitySet{})); // empty effective caps

            CHECK(registry.TryGet<RegSubA>() != nullptr);

            registry.Shutdown();
        }

        TEST_CASE("Fail-fast on Initialise error with reverse cleanup")
        {
            LifecycleLog log;
            s_log = &log;

            TestEngineRuntime runtime;
            auto ctx = runtime.GetContext();

            SubsystemRegistry<AppSubsystem> registry;
            registry.Register<RegSubA>();
            // FailingSub depends on A so it inits second
            registry.Register<FailingSub>({.DependsOn = Deps<RegSubA>()});

            REQUIRE(registry.Finalise());
            auto result = registry.Initialise(ctx, CapabilitySet{});

            // Should have failed
            CHECK(not result.has_value());
            CHECK(result.error().GetMessage() == "FailingSub init failed");

            // A was initialised, then Fail attempted, then A was shut down
            REQUIRE(log.Events.size() == 3);
            CHECK(log.Events[0] == "A.Init");
            CHECK(log.Events[1] == "Fail.Init");
            CHECK(log.Events[2] == "A.Shutdown");

            s_log = nullptr;
        }

        TEST_CASE("TryGet returns nullptr for unregistered type")
        {
            TestEngineRuntime runtime;
            auto ctx = runtime.GetContext();

            SubsystemRegistry<AppSubsystem> registry;
            REQUIRE(registry.Finalise());
            REQUIRE(registry.Initialise(ctx, CapabilitySet{}));

            CHECK(registry.TryGet<RegSubA>() == nullptr);

            registry.Shutdown();
        }

        TEST_CASE("Deps helper produces correct type indices")
        {
            auto deps = Deps<RegSubA, RegSubB>();
            REQUIRE(deps.size() == 2);
            CHECK(deps[0] == std::type_index(typeid(RegSubA)));
            CHECK(deps[1] == std::type_index(typeid(RegSubB)));
        }

        TEST_CASE("IsRegistered returns true for registered type")
        {
            SubsystemRegistry<AppSubsystem> registry;
            registry.Register<RegSubA>();

            CHECK(registry.IsRegistered<RegSubA>());
            CHECK(not registry.IsRegistered<RegSubB>());
        }

        TEST_CASE("IsRegistered works for abstract type")
        {
            SubsystemRegistry<AppSubsystem> registry;
            registry.Register<ConcreteService, IAbstractService>();

            CHECK(registry.IsRegistered<ConcreteService>());
            CHECK(registry.IsRegistered<IAbstractService>());
            CHECK(not registry.IsRegistered<RegSubA>());
        }

        TEST_CASE("IsFinalised reflects registry state")
        {
            SubsystemRegistry<AppSubsystem> registry;
            CHECK(not registry.IsFinalised());

            registry.Register<RegSubA>();
            REQUIRE(registry.Finalise());

            CHECK(registry.IsFinalised());
        }

        TEST_CASE("Const Get returns const reference")
        {
            TestEngineRuntime runtime;
            auto ctx = runtime.GetContext();

            SubsystemRegistry<AppSubsystem> registry;
            registry.Register<ConcreteService, IAbstractService>();
            REQUIRE(registry.Finalise());
            REQUIRE(registry.Initialise(ctx, CapabilitySet{}));

            const auto& constRegistry = registry;
            const auto* ptr = constRegistry.TryGet<IAbstractService>();
            REQUIRE(ptr != nullptr);
            CHECK(ptr->GetValue() == 42);

            registry.Shutdown();
        }

        TEST_CASE("Multiple independent subsystems all initialise")
        {
            LifecycleLog log;
            s_log = &log;

            TestEngineRuntime runtime;
            auto ctx = runtime.GetContext();

            SubsystemRegistry<AppSubsystem> registry;
            registry.Register<RegSubA>();
            registry.Register<RegSubB>();
            registry.Register<RegSubC>();

            REQUIRE(registry.Finalise());
            REQUIRE(registry.Initialise(ctx, CapabilitySet{}));

            CHECK(registry.TryGet<RegSubA>() != nullptr);
            CHECK(registry.TryGet<RegSubB>() != nullptr);
            CHECK(registry.TryGet<RegSubC>() != nullptr);
            CHECK(log.Events.size() == 3);

            registry.Shutdown();
            s_log = nullptr;
        }
    }
}