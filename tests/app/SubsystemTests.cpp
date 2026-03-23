#include "app/Subsystem.h"

#include <doctest/doctest.h>

#include <string>
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
            if (s_log) s_log->Events.push_back("A.Init");
        }
        void Shutdown() override
        {
            if (s_log) s_log->Events.push_back("A.Shutdown");
        }
    };

    class SubsystemB : public GameSubsystem
    {
    public:
        void Initialise() override
        {
            if (s_log) s_log->Events.push_back("B.Init");
        }
        void Shutdown() override
        {
            if (s_log) s_log->Events.push_back("B.Shutdown");
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
            if (s_log) s_log->Events.push_back("C.Init");
        }
        void Shutdown() override
        {
            if (s_log) s_log->Events.push_back("C.Shutdown");
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
            collection.Register<SubsystemA>([]() -> bool { return false; });
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
}