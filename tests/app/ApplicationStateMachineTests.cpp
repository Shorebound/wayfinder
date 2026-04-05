#include "app/ApplicationStateMachine.h"

#include "app/EngineContext.h"
#include "app/IApplicationState.h"
#include "app/OrchestrationTypes.h"
#include "plugins/IStateUI.h"
#include "plugins/LifecycleHooks.h"

#include <doctest/doctest.h>

namespace Wayfinder::Tests
{
    // -- Mock states ------------------------------------------------------

    class MockStateA : public IApplicationState
    {
    public:
        bool EnterCalled = false;
        bool ExitCalled = false;
        bool SuspendCalled = false;
        bool ResumeCalled = false;
        bool ShouldFailOnEnter = false;

        auto OnEnter(EngineContext& /*context*/) -> Result<void> override
        {
            EnterCalled = true;
            if (ShouldFailOnEnter)
            {
                return MakeError("MockStateA enter failed");
            }
            return {};
        }

        auto OnExit(EngineContext& /*context*/) -> Result<void> override
        {
            ExitCalled = true;
            return {};
        }

        void OnSuspend(EngineContext& /*context*/) override
        {
            SuspendCalled = true;
        }
        void OnResume(EngineContext& /*context*/) override
        {
            ResumeCalled = true;
        }

        [[nodiscard]] auto GetName() const -> std::string_view override
        {
            return "MockStateA";
        }
    };

    class MockStateB : public IApplicationState
    {
    public:
        bool EnterCalled = false;
        bool ExitCalled = false;
        bool SuspendCalled = false;
        bool ResumeCalled = false;
        bool ShouldFailOnEnter = false;

        auto OnEnter(EngineContext& /*context*/) -> Result<void> override
        {
            EnterCalled = true;
            if (ShouldFailOnEnter)
            {
                return MakeError("MockStateB enter failed");
            }
            return {};
        }

        auto OnExit(EngineContext& /*context*/) -> Result<void> override
        {
            ExitCalled = true;
            return {};
        }

        void OnSuspend(EngineContext& /*context*/) override
        {
            SuspendCalled = true;
        }
        void OnResume(EngineContext& /*context*/) override
        {
            ResumeCalled = true;
        }

        [[nodiscard]] auto GetName() const -> std::string_view override
        {
            return "MockStateB";
        }
    };

    class MockStateC : public IApplicationState
    {
    public:
        bool EnterCalled = false;
        bool ExitCalled = false;

        auto OnEnter(EngineContext& /*context*/) -> Result<void> override
        {
            EnterCalled = true;
            return {};
        }

        auto OnExit(EngineContext& /*context*/) -> Result<void> override
        {
            ExitCalled = true;
            return {};
        }

        [[nodiscard]] auto GetName() const -> std::string_view override
        {
            return "MockStateC";
        }
    };

    /// A state that wants to keep rendering in the background when suspended.
    class MockPausableState : public IApplicationState
    {
    public:
        bool EnterCalled = false;
        bool ExitCalled = false;
        bool SuspendCalled = false;
        bool ResumeCalled = false;

        auto OnEnter(EngineContext& /*context*/) -> Result<void> override
        {
            EnterCalled = true;
            return {};
        }

        auto OnExit(EngineContext& /*context*/) -> Result<void> override
        {
            ExitCalled = true;
            return {};
        }

        void OnSuspend(EngineContext& /*context*/) override
        {
            SuspendCalled = true;
        }
        void OnResume(EngineContext& /*context*/) override
        {
            ResumeCalled = true;
        }

        [[nodiscard]] auto GetBackgroundPreferences() const -> BackgroundPreferences override
        {
            return {.WantsBackgroundUpdate = false, .WantsBackgroundRender = true};
        }

        [[nodiscard]] auto GetName() const -> std::string_view override
        {
            return "MockPausableState";
        }
    };

    /// A pause-menu state that allows background rendering but not updating.
    class MockPauseState : public IApplicationState
    {
    public:
        bool EnterCalled = false;
        bool ExitCalled = false;

        auto OnEnter(EngineContext& /*context*/) -> Result<void> override
        {
            EnterCalled = true;
            return {};
        }

        auto OnExit(EngineContext& /*context*/) -> Result<void> override
        {
            ExitCalled = true;
            return {};
        }

        [[nodiscard]] auto GetSuspensionPolicy() const -> SuspensionPolicy override
        {
            return {.AllowBackgroundUpdate = false, .AllowBackgroundRender = true};
        }

        [[nodiscard]] auto GetName() const -> std::string_view override
        {
            return "MockPauseState";
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

        auto OnAttach(EngineContext& /*context*/) -> Result<void> override
        {
            AttachCalled = true;
            return {};
        }

        auto OnDetach(EngineContext& /*context*/) -> Result<void> override
        {
            DetachCalled = true;
            return {};
        }

        void OnSuspend(EngineContext& /*context*/) override
        {
            SuspendCalled = true;
        }
        void OnResume(EngineContext& /*context*/) override
        {
            ResumeCalled = true;
        }

        [[nodiscard]] auto GetName() const -> std::string_view override
        {
            return "MockStateUI";
        }
    };

    // -- Helper -----------------------------------------------------------

    /// Create a minimal ASM with StateA (initial) and StateB, transitions A<->B, finalised.
    auto MakeASM() -> ApplicationStateMachine
    {
        ApplicationStateMachine asm_;
        asm_.AddState<MockStateA>();
        asm_.AddState<MockStateB>();
        asm_.SetInitialState<MockStateA>();
        asm_.AddTransition<MockStateA, MockStateB>();
        asm_.AddTransition<MockStateB, MockStateA>();
        auto result = asm_.Finalise();
        REQUIRE(result.has_value());
        return asm_;
    }

    // -- Tests ------------------------------------------------------------

    TEST_SUITE("ApplicationStateMachine")
    {
        TEST_CASE("Finalise succeeds with valid state graph")
        {
            ApplicationStateMachine asm_;
            asm_.AddState<MockStateA>();
            asm_.AddState<MockStateB>();
            asm_.SetInitialState<MockStateA>();
            asm_.AddTransition<MockStateA, MockStateB>();
            asm_.AddTransition<MockStateB, MockStateA>();

            auto result = asm_.Finalise();
            CHECK(result.has_value());
            CHECK(asm_.IsFinalised());
        }

        TEST_CASE("Finalise fails when initial state not registered")
        {
            ApplicationStateMachine asm_;
            asm_.AddState<MockStateA>();
            asm_.SetInitialState<MockStateB>(); // B not registered

            auto result = asm_.Finalise();
            CHECK_FALSE(result.has_value());
            CHECK(result.error().GetMessage().find("not registered") != std::string::npos);
        }

        TEST_CASE("Finalise fails when transition target not registered")
        {
            ApplicationStateMachine asm_;
            asm_.AddState<MockStateA>();
            asm_.SetInitialState<MockStateA>();
            asm_.AddTransition<MockStateA, MockStateB>(); // B not registered

            auto result = asm_.Finalise();
            CHECK_FALSE(result.has_value());
        }

        TEST_CASE("Finalise detects unreachable states")
        {
            ApplicationStateMachine asm_;
            asm_.AddState<MockStateA>();
            asm_.AddState<MockStateB>();
            asm_.AddState<MockStateC>();
            asm_.SetInitialState<MockStateA>();
            asm_.AddTransition<MockStateA, MockStateB>();
            asm_.AddTransition<MockStateB, MockStateA>();
            // C is unreachable

            auto result = asm_.Finalise();
            CHECK_FALSE(result.has_value());
            CHECK(result.error().GetMessage().find("Unreachable") != std::string::npos);
        }

        TEST_CASE("Start enters initial state")
        {
            auto asm_ = MakeASM();
            EngineContext ctx;

            asm_.Start(ctx);
            CHECK(asm_.IsRunning());
            CHECK(asm_.GetActiveState() != nullptr);

            auto* state = dynamic_cast<MockStateA*>(asm_.GetActiveState());
            REQUIRE(state != nullptr);
            CHECK(state->EnterCalled);
        }

        TEST_CASE("Flat transition exits old state and enters new state")
        {
            auto asm_ = MakeASM();
            EngineContext ctx;
            asm_.Start(ctx);

            asm_.RequestTransition<MockStateB>();
            asm_.ProcessPending(ctx);

            CHECK(asm_.GetActiveStateType() == std::type_index(typeid(MockStateB)));
        }

        TEST_CASE("Transitions are deferred until ProcessPending")
        {
            auto asm_ = MakeASM();
            EngineContext ctx;
            asm_.Start(ctx);

            asm_.RequestTransition<MockStateB>();
            // Before ProcessPending: still on A.
            CHECK(asm_.GetActiveStateType() == std::type_index(typeid(MockStateA)));

            asm_.ProcessPending(ctx);
            // After ProcessPending: now on B.
            CHECK(asm_.GetActiveStateType() == std::type_index(typeid(MockStateB)));
        }

        TEST_CASE("Push suspends current state and enters pushed state")
        {
            ApplicationStateMachine asm_;
            asm_.AddState<MockStateA>();
            asm_.AddState<MockStateB>();
            asm_.SetInitialState<MockStateA>();
            asm_.AddTransition<MockStateA, MockStateB>();
            asm_.AddTransition<MockStateB, MockStateA>();
            asm_.AllowPush<MockStateB>();
            REQUIRE(asm_.Finalise().has_value());

            EngineContext ctx;
            asm_.Start(ctx);

            asm_.RequestPush<MockStateB>();
            asm_.ProcessPending(ctx);

            CHECK(asm_.GetActiveStateType() == std::type_index(typeid(MockStateB)));
            CHECK(asm_.GetModalStack().size() == 2);
        }

        TEST_CASE("Pop exits pushed state and resumes previous")
        {
            ApplicationStateMachine asm_;
            asm_.AddState<MockStateA>();
            asm_.AddState<MockStateB>();
            asm_.SetInitialState<MockStateA>();
            asm_.AddTransition<MockStateA, MockStateB>();
            asm_.AddTransition<MockStateB, MockStateA>();
            asm_.AllowPush<MockStateB>();
            REQUIRE(asm_.Finalise().has_value());

            EngineContext ctx;
            asm_.Start(ctx);
            asm_.RequestPush<MockStateB>();
            asm_.ProcessPending(ctx);

            asm_.RequestPop();
            asm_.ProcessPending(ctx);

            CHECK(asm_.GetActiveStateType() == std::type_index(typeid(MockStateA)));
            CHECK(asm_.GetModalStack().size() == 1);
        }

        TEST_CASE("Push/pop negotiation computes correct background policy")
        {
            ApplicationStateMachine asm_;
            asm_.AddState<MockPausableState>();
            asm_.AddState<MockPauseState>();
            asm_.SetInitialState<MockPausableState>();
            asm_.AllowPush<MockPauseState>();
            REQUIRE(asm_.Finalise().has_value());

            EngineContext ctx;
            asm_.Start(ctx);
            asm_.RequestPush<MockPauseState>();
            asm_.ProcessPending(ctx);

            // Background = PausableState: WantsUpdate=false, WantsRender=true
            // Foreground = PauseState: AllowUpdate=false, AllowRender=true
            // AND intersection: Update = false AND false = false, Render = true AND true = true
            const auto* policy = asm_.GetBackgroundPolicy(std::type_index(typeid(MockPausableState)));
            REQUIRE(policy != nullptr);
            CHECK_FALSE(policy->Update);
            CHECK(policy->Render);
        }

        TEST_CASE("State subsystems persist across push/pop without exit/enter")
        {
            ApplicationStateMachine asm_;
            asm_.AddState<MockStateA>();
            asm_.AddState<MockStateB>();
            asm_.SetInitialState<MockStateA>();
            asm_.AddTransition<MockStateA, MockStateB>();
            asm_.AddTransition<MockStateB, MockStateA>();
            asm_.AllowPush<MockStateB>();
            REQUIRE(asm_.Finalise().has_value());

            EngineContext ctx;
            asm_.Start(ctx);
            auto* stateA = dynamic_cast<MockStateA*>(asm_.GetActiveState());
            REQUIRE(stateA != nullptr);
            CHECK(stateA->EnterCalled);
            stateA->EnterCalled = false; // Reset for re-check.

            asm_.RequestPush<MockStateB>();
            asm_.ProcessPending(ctx);
            // A should be suspended, NOT exited.
            CHECK(stateA->SuspendCalled);
            CHECK_FALSE(stateA->ExitCalled);

            asm_.RequestPop();
            asm_.ProcessPending(ctx);
            // A should be resumed, NOT re-entered.
            CHECK(stateA->ResumeCalled);
            CHECK_FALSE(stateA->EnterCalled); // Should NOT have been re-entered.
        }

        TEST_CASE("Last-write-wins for multiple pending transitions")
        {
            auto asm_ = MakeASM();
            EngineContext ctx;
            asm_.Start(ctx);

            asm_.RequestTransition<MockStateB>();
            asm_.RequestTransition<MockStateA>(); // Overwrites.
            asm_.ProcessPending(ctx);

            CHECK(asm_.GetActiveStateType() == std::type_index(typeid(MockStateA)));
        }

        TEST_CASE("IStateUI lifecycle mirrors state on flat transition")
        {
            auto asm_ = MakeASM();

            // Use a raw pointer to track the MockStateUI instance.
            MockStateUI* uiPtr = nullptr;
            asm_.RegisterStateUI(std::type_index(typeid(MockStateA)), [&uiPtr]() -> std::unique_ptr<IStateUI>
            {
                auto ui = std::make_unique<MockStateUI>();
                uiPtr = ui.get();
                return ui;
            });

            EngineContext ctx;
            asm_.Start(ctx);
            REQUIRE(uiPtr != nullptr);
            CHECK(uiPtr->AttachCalled);

            asm_.RequestTransition<MockStateB>();
            asm_.ProcessPending(ctx);
            CHECK(uiPtr->DetachCalled);
        }

        TEST_CASE("IStateUI lifecycle mirrors state on push/pop")
        {
            ApplicationStateMachine asm_;
            asm_.AddState<MockStateA>();
            asm_.AddState<MockStateB>();
            asm_.SetInitialState<MockStateA>();
            asm_.AddTransition<MockStateA, MockStateB>();
            asm_.AddTransition<MockStateB, MockStateA>();
            asm_.AllowPush<MockStateB>();
            REQUIRE(asm_.Finalise().has_value());

            MockStateUI* uiPtr = nullptr;
            asm_.RegisterStateUI(std::type_index(typeid(MockStateA)), [&uiPtr]() -> std::unique_ptr<IStateUI>
            {
                auto ui = std::make_unique<MockStateUI>();
                uiPtr = ui.get();
                return ui;
            });

            EngineContext ctx;
            asm_.Start(ctx);
            REQUIRE(uiPtr != nullptr);
            CHECK(uiPtr->AttachCalled);

            asm_.RequestPush<MockStateB>();
            asm_.ProcessPending(ctx);
            CHECK(uiPtr->SuspendCalled);

            asm_.RequestPop();
            asm_.ProcessPending(ctx);
            CHECK(uiPtr->ResumeCalled);
        }

        TEST_CASE("Lifecycle hooks fire during transitions")
        {
            auto asm_ = MakeASM();

            bool enterHookFired = false;
            bool exitHookFired = false;
            LifecycleHookManifest hooks;
            hooks.OnStateEnter[std::type_index(typeid(MockStateB))].push_back([&enterHookFired](EngineContext& /*ctx*/)
            {
                enterHookFired = true;
            });
            hooks.OnStateExit[std::type_index(typeid(MockStateA))].push_back([&exitHookFired](EngineContext& /*ctx*/)
            {
                exitHookFired = true;
            });
            asm_.SetLifecycleHooks(&hooks);

            EngineContext ctx;
            asm_.Start(ctx);
            asm_.RequestTransition<MockStateB>();
            asm_.ProcessPending(ctx);

            CHECK(enterHookFired);
            CHECK(exitHookFired);
        }

        TEST_CASE("Flat transition does not crash on enter")
        {
            ApplicationStateMachine asm_;
            asm_.AddState<MockStateA>();
            asm_.AddState<MockStateB>();
            asm_.SetInitialState<MockStateA>();
            asm_.AddTransition<MockStateA, MockStateB>();
            asm_.AddTransition<MockStateB, MockStateA>();
            REQUIRE(asm_.Finalise().has_value());

            EngineContext ctx;
            asm_.Start(ctx);

            // Verify the ASM transitions cleanly.
            asm_.RequestTransition<MockStateB>();
            asm_.ProcessPending(ctx);
            CHECK(asm_.GetActiveStateType() == std::type_index(typeid(MockStateB)));
        }

        TEST_CASE("Shutdown exits all states in stack order")
        {
            ApplicationStateMachine asm_;
            asm_.AddState<MockStateA>();
            asm_.AddState<MockStateB>();
            asm_.SetInitialState<MockStateA>();
            asm_.AddTransition<MockStateA, MockStateB>();
            asm_.AddTransition<MockStateB, MockStateA>();
            asm_.AllowPush<MockStateB>();
            REQUIRE(asm_.Finalise().has_value());

            EngineContext ctx;
            asm_.Start(ctx);
            asm_.RequestPush<MockStateB>();
            asm_.ProcessPending(ctx);
            CHECK(asm_.GetModalStack().size() == 2);

            asm_.Shutdown(ctx);
            CHECK_FALSE(asm_.IsRunning());
            CHECK(asm_.GetModalStack().empty());
        }

        TEST_CASE("GetModalStack reflects current stack state")
        {
            ApplicationStateMachine asm_;
            asm_.AddState<MockStateA>();
            asm_.AddState<MockStateB>();
            asm_.SetInitialState<MockStateA>();
            asm_.AddTransition<MockStateA, MockStateB>();
            asm_.AddTransition<MockStateB, MockStateA>();
            asm_.AllowPush<MockStateB>();
            REQUIRE(asm_.Finalise().has_value());

            EngineContext ctx;
            asm_.Start(ctx);
            auto stack = asm_.GetModalStack();
            REQUIRE(stack.size() == 1);
            CHECK(stack[0] == std::type_index(typeid(MockStateA)));

            asm_.RequestPush<MockStateB>();
            asm_.ProcessPending(ctx);
            stack = asm_.GetModalStack();
            REQUIRE(stack.size() == 2);
            CHECK(stack[0] == std::type_index(typeid(MockStateA)));
            CHECK(stack[1] == std::type_index(typeid(MockStateB)));
        }

        TEST_CASE("Pushable states make otherwise unreachable states reachable")
        {
            ApplicationStateMachine asm_;
            asm_.AddState<MockStateA>();
            asm_.AddState<MockStateB>();
            asm_.SetInitialState<MockStateA>();
            // No flat transitions from A to B, but B is pushable.
            asm_.AllowPush<MockStateB>();

            auto result = asm_.Finalise();
            CHECK(result.has_value()); // B is reachable via push.
        }

        TEST_CASE("ProcessPending with no pending operation is a no-op")
        {
            auto asm_ = MakeASM();
            EngineContext ctx;
            asm_.Start(ctx);

            auto typeBefore = asm_.GetActiveStateType();
            asm_.ProcessPending(ctx); // No pending operation.
            CHECK(asm_.GetActiveStateType() == typeBefore);
        }

        TEST_CASE("GetStateCapabilities returns registered capabilities")
        {
            ApplicationStateMachine asm_;
            asm_.AddState<MockStateA>(); // Default empty capabilities.
            asm_.SetInitialState<MockStateA>();
            REQUIRE(asm_.Finalise().has_value());

            const auto* caps = asm_.GetStateCapabilities(std::type_index(typeid(MockStateA)));
            REQUIRE(caps != nullptr);
            CHECK(caps->IsEmpty());
        }
    }

} // namespace Wayfinder::Tests
