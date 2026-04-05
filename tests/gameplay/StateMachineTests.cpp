#include <doctest/doctest.h>

#include "core/InternedString.h"
#include "gameplay/StateMachine.h"

#include <string>
#include <vector>

using namespace Wayfinder;

// ---------------------------------------------------------------------------
// Helper enum class for testing with enum keys
// ---------------------------------------------------------------------------

enum class TestState
{
    Menu,
    Playing,
    Paused,
    GameOver
};

struct TestStateHash
{
    size_t operator()(TestState s) const noexcept
    {
        return std::hash<std::underlying_type_t<TestState>>{}(std::to_underlying(s));
    }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static auto CreateSimpleMachine() -> StateMachine<TestState, TestStateHash>
{
    StateMachine<TestState, TestStateHash> sm;
    sm.AddState({.Id = TestState::Menu, .AllowedTransitions = {TestState::Playing}});
    sm.AddState({.Id = TestState::Playing, .AllowedTransitions = {TestState::Paused, TestState::GameOver}});
    sm.AddState({.Id = TestState::Paused, .AllowedTransitions = {TestState::Playing}});
    sm.AddState({.Id = TestState::GameOver, .AllowedTransitions = {TestState::Menu}});
    return sm;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_SUITE("StateMachine")
{

    TEST_CASE("Finalise succeeds with valid graph")
    {
        auto sm = CreateSimpleMachine();
        auto result = sm.Finalise(TestState::Menu);
        CHECK(result.has_value());
    }

    TEST_CASE("Finalise rejects unregistered initial state")
    {
        StateMachine<TestState, TestStateHash> sm;
        auto result = sm.Finalise(TestState::Menu);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().GetMessage().find("Initial state") != std::string::npos);
    }

    TEST_CASE("Finalise rejects dangling transition target")
    {
        StateMachine<TestState, TestStateHash> sm;
        sm.AddState({
            .Id = TestState::Menu, .AllowedTransitions = {TestState::Playing} // Playing not registered
        });
        auto result = sm.Finalise(TestState::Menu);
        CHECK_FALSE(result.has_value());
    }

    TEST_CASE("Finalise rejects unreachable states")
    {
        StateMachine<TestState, TestStateHash> sm;
        sm.AddState({.Id = TestState::Menu, .AllowedTransitions = {TestState::Playing}});
        sm.AddState({.Id = TestState::Playing, .AllowedTransitions = {TestState::Menu}});
        // GameOver is registered but unreachable from Menu
        sm.AddState({.Id = TestState::GameOver, .AllowedTransitions = {TestState::Menu}});
        auto result = sm.Finalise(TestState::Menu);
        CHECK_FALSE(result.has_value());
    }

    TEST_CASE("Start calls OnEnter for initial state")
    {
        std::vector<std::string> log;

        StateMachine<TestState, TestStateHash> sm;
        sm.AddState({.Id = TestState::Menu,
            .OnEnter =
                [&]
        {
            log.push_back("enter_menu");
        },
            .AllowedTransitions = {TestState::Playing}});
        sm.AddState({.Id = TestState::Playing, .AllowedTransitions = {TestState::Menu}});

        auto result = sm.Finalise(TestState::Menu);
        REQUIRE(result.has_value());
        sm.Start();

        REQUIRE(log.size() == 1);
        CHECK(log[0] == "enter_menu");
        CHECK(sm.IsRunning());
        CHECK(sm.GetCurrentState() == TestState::Menu);
    }

    TEST_CASE("TransitionTo and ProcessPending fire callbacks in order")
    {
        std::vector<std::string> log;

        StateMachine<TestState, TestStateHash> sm;
        sm.AddState({.Id = TestState::Menu,
            .OnEnter =
                [&]
        {
            log.push_back("enter_menu");
        },
            .OnExit =
                [&]
        {
            log.push_back("exit_menu");
        },
            .AllowedTransitions = {TestState::Playing}});
        sm.AddState({.Id = TestState::Playing,
            .OnEnter =
                [&]
        {
            log.push_back("enter_playing");
        },
            .OnExit =
                [&]
        {
            log.push_back("exit_playing");
        },
            .AllowedTransitions = {TestState::Menu}});

        REQUIRE(sm.Finalise(TestState::Menu).has_value());
        sm.Start();
        log.clear();

        REQUIRE(sm.TransitionTo(TestState::Playing).has_value());
        CHECK(log.empty());

        sm.ProcessPending();
        REQUIRE(log.size() == 2);
        CHECK(log[0] == "exit_menu");
        CHECK(log[1] == "enter_playing");
    }

    TEST_CASE("TransitionTo without ProcessPending does not fire callbacks")
    {
        std::vector<std::string> log;

        StateMachine<TestState, TestStateHash> sm;
        sm.AddState({.Id = TestState::Menu,
            .OnExit =
                [&]
        {
            log.push_back("exit_menu");
        },
            .AllowedTransitions = {TestState::Playing}});
        sm.AddState({.Id = TestState::Playing,
            .OnEnter =
                [&]
        {
            log.push_back("enter_playing");
        },
            .AllowedTransitions = {TestState::Menu}});

        REQUIRE(sm.Finalise(TestState::Menu).has_value());
        sm.Start();
        log.clear();

        REQUIRE(sm.TransitionTo(TestState::Playing).has_value());
        CHECK(log.empty());
        CHECK(sm.GetCurrentState() == TestState::Menu);
    }

    TEST_CASE("GetCurrentState and GetPreviousState track correctly")
    {
        auto sm = CreateSimpleMachine();
        REQUIRE(sm.Finalise(TestState::Menu).has_value());
        sm.Start();

        CHECK(sm.GetCurrentState() == TestState::Menu);
        CHECK_FALSE(sm.GetPreviousState().has_value());

        REQUIRE(sm.TransitionTo(TestState::Playing).has_value());
        sm.ProcessPending();
        CHECK(sm.GetCurrentState() == TestState::Playing);
        REQUIRE(sm.GetPreviousState().has_value());
        CHECK(sm.GetPreviousState().value() == TestState::Menu);

        REQUIRE(sm.TransitionTo(TestState::GameOver).has_value());
        sm.ProcessPending();
        CHECK(sm.GetCurrentState() == TestState::GameOver);
        CHECK(sm.GetPreviousState().value() == TestState::Playing);
    }

    TEST_CASE("Transition observers fire after OnEnter")
    {
        std::vector<std::string> log;

        StateMachine<TestState, TestStateHash> sm;
        sm.AddState({.Id = TestState::Menu,
            .OnEnter =
                [&]
        {
            log.push_back("enter_menu");
        },
            .OnExit =
                [&]
        {
            log.push_back("exit_menu");
        },
            .AllowedTransitions = {TestState::Playing}});
        sm.AddState({.Id = TestState::Playing,
            .OnEnter =
                [&]
        {
            log.push_back("enter_playing");
        },
            .AllowedTransitions = {TestState::Menu}});

        sm.OnTransition([&](const TestState& /*from*/, const TestState& /*to*/)
        {
            log.push_back("observer");
        });

        REQUIRE(sm.Finalise(TestState::Menu).has_value());
        sm.Start();
        log.clear();

        REQUIRE(sm.TransitionTo(TestState::Playing).has_value());
        sm.ProcessPending();

        REQUIRE(log.size() == 3);
        CHECK(log[0] == "exit_menu");
        CHECK(log[1] == "enter_playing");
        CHECK(log[2] == "observer");
    }

    TEST_CASE("Multiple TransitionTo before ProcessPending uses last")
    {
        auto sm = CreateSimpleMachine();
        REQUIRE(sm.Finalise(TestState::Menu).has_value());
        sm.Start();

        // Transition to Playing first (Menu -> Playing is allowed)
        REQUIRE(sm.TransitionTo(TestState::Playing).has_value());
        sm.ProcessPending();

        // Now from Playing, both Paused and GameOver are allowed
        REQUIRE(sm.TransitionTo(TestState::Paused).has_value());
        REQUIRE(sm.TransitionTo(TestState::GameOver).has_value()); // Last write wins
        sm.ProcessPending();

        CHECK(sm.GetCurrentState() == TestState::GameOver);
    }

    TEST_CASE("Works with InternedString as TStateId")
    {
        auto menu = InternedString::Intern("Menu");
        auto playing = InternedString::Intern("Playing");

        StateMachine<InternedString> sm;
        sm.AddState({.Id = menu, .AllowedTransitions = {playing}});
        sm.AddState({.Id = playing, .AllowedTransitions = {menu}});

        auto result = sm.Finalise(menu);
        CHECK(result.has_value());

        sm.Start();
        CHECK(sm.GetCurrentState() == menu);

        REQUIRE(sm.TransitionTo(playing).has_value());
        sm.ProcessPending();
        CHECK(sm.GetCurrentState() == playing);
    }

    TEST_CASE("Works with enum class as TStateId")
    {
        StateMachine<TestState, TestStateHash> sm;
        sm.AddState({.Id = TestState::Menu, .AllowedTransitions = {TestState::Playing}});
        sm.AddState({.Id = TestState::Playing, .AllowedTransitions = {TestState::Menu}});

        auto result = sm.Finalise(TestState::Menu);
        CHECK(result.has_value());

        sm.Start();
        CHECK(sm.GetCurrentState() == TestState::Menu);

        REQUIRE(sm.TransitionTo(TestState::Playing).has_value());
        sm.ProcessPending();
        CHECK(sm.GetCurrentState() == TestState::Playing);
    }

} // TEST_SUITE("StateMachine")
