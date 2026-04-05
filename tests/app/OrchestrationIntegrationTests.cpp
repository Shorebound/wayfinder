#include "app/AppBuilder.h"
#include "app/ApplicationStateMachine.h"
#include "app/EngineContext.h"
#include "app/IApplicationState.h"
#include "app/IOverlay.h"
#include "app/OrchestrationTypes.h"
#include "app/OverlayManifest.h"
#include "app/OverlayStack.h"
#include "app/StateManifest.h"
#include "gameplay/Capability.h"
#include "gameplay/NativeTag.h"
#include "gameplay/TagRegistry.h"
#include "plugins/IStateUI.h"
#include "plugins/LifecycleHooks.h"

#include <doctest/doctest.h>

using namespace Wayfinder;

namespace Wayfinder::Tests
{
    // -- Integration mock types -------------------------------------------

    class IntegStateA : public IApplicationState
    {
    public:
        bool Entered = false;
        bool Exited = false;

        [[nodiscard]] auto OnEnter(EngineContext& /*context*/) -> Result<void> override
        {
            Entered = true;
            return {};
        }
        [[nodiscard]] auto OnExit(EngineContext& /*context*/) -> Result<void> override
        {
            Exited = true;
            return {};
        }
        [[nodiscard]] auto GetBackgroundPreferences() const -> BackgroundMode override
        {
            return BackgroundMode::Render;
        }
        [[nodiscard]] auto GetName() const -> std::string_view override
        {
            return "IntegStateA";
        }
    };

    class IntegStateB : public IApplicationState
    {
    public:
        bool Entered = false;
        bool Exited = false;

        [[nodiscard]] auto OnEnter(EngineContext& /*context*/) -> Result<void> override
        {
            Entered = true;
            return {};
        }
        [[nodiscard]] auto OnExit(EngineContext& /*context*/) -> Result<void> override
        {
            Exited = true;
            return {};
        }
        [[nodiscard]] auto GetSuspensionPolicy() const -> BackgroundMode override
        {
            return BackgroundMode::Render;
        }
        [[nodiscard]] auto GetName() const -> std::string_view override
        {
            return "IntegStateB";
        }
    };

    class IntegOverlay : public IOverlay
    {
    public:
        bool Attached = false;
        bool Detached = false;
        bool Updated = false;

        [[nodiscard]] auto OnAttach(EngineContext& /*context*/) -> Result<void> override
        {
            Attached = true;
            return {};
        }
        [[nodiscard]] auto OnDetach(EngineContext& /*context*/) -> Result<void> override
        {
            Detached = true;
            return {};
        }
        void OnUpdate(EngineContext& /*context*/, float /*deltaTime*/) override
        {
            Updated = true;
        }
        [[nodiscard]] auto GetName() const -> std::string_view override
        {
            return "IntegOverlay";
        }
    };

    class IntegStateUI : public IStateUI
    {
    public:
        bool Attached = false;
        bool Detached = false;
        bool Suspended = false;
        bool Resumed = false;

        [[nodiscard]] auto OnAttach(EngineContext& /*context*/) -> Result<void> override
        {
            Attached = true;
            return {};
        }
        [[nodiscard]] auto OnDetach(EngineContext& /*context*/) -> Result<void> override
        {
            Detached = true;
            return {};
        }
        void OnSuspend(EngineContext& /*context*/) override
        {
            Suspended = true;
        }
        void OnResume(EngineContext& /*context*/) override
        {
            Resumed = true;
        }
        [[nodiscard]] auto GetName() const -> std::string_view override
        {
            return "IntegStateUI";
        }
    };

    // Static pointers for accessing mock instances created by factories.
    // Reset before each test that uses them.
    static IntegStateUI* s_lastStateUI = nullptr; // NOLINT

    TEST_SUITE("Orchestration Integration")
    {
        TEST_CASE("EngineContext.RequestTransition delegates to ASM")
        {
            ApplicationStateMachine asm_;
            asm_.AddState<IntegStateA>();
            asm_.AddState<IntegStateB>();
            asm_.SetInitialState<IntegStateA>();
            asm_.AddTransition<IntegStateA, IntegStateB>();
            auto finaliseResult = asm_.Finalise();
            REQUIRE(finaliseResult.has_value());

            EngineContext ctx;
            ctx.SetStateMachine(&asm_);
            asm_.Start(ctx);

            ctx.RequestTransition<IntegStateB>();
            asm_.ProcessPending(ctx);

            CHECK(asm_.GetActiveStateType() == std::type_index(typeid(IntegStateB)));
        }

        TEST_CASE("EngineContext.RequestPush delegates to ASM")
        {
            ApplicationStateMachine asm_;
            asm_.AddState<IntegStateA>();
            asm_.AddState<IntegStateB>();
            asm_.SetInitialState<IntegStateA>();
            asm_.AllowPush<IntegStateB>();
            auto finaliseResult = asm_.Finalise();
            REQUIRE(finaliseResult.has_value());

            EngineContext ctx;
            ctx.SetStateMachine(&asm_);
            asm_.Start(ctx);

            ctx.RequestPush<IntegStateB>();
            asm_.ProcessPending(ctx);

            CHECK(asm_.GetModalStack().size() == 2);
            CHECK(asm_.GetActiveStateType() == std::type_index(typeid(IntegStateB)));
        }

        TEST_CASE("EngineContext.RequestPop delegates to ASM")
        {
            ApplicationStateMachine asm_;
            asm_.AddState<IntegStateA>();
            asm_.AddState<IntegStateB>();
            asm_.SetInitialState<IntegStateA>();
            asm_.AllowPush<IntegStateB>();
            auto finaliseResult = asm_.Finalise();
            REQUIRE(finaliseResult.has_value());

            EngineContext ctx;
            ctx.SetStateMachine(&asm_);
            asm_.Start(ctx);

            ctx.RequestPush<IntegStateB>();
            asm_.ProcessPending(ctx);
            REQUIRE(asm_.GetModalStack().size() == 2);

            ctx.RequestPop();
            asm_.ProcessPending(ctx);

            CHECK(asm_.GetModalStack().size() == 1);
            CHECK(asm_.GetActiveStateType() == std::type_index(typeid(IntegStateA)));
        }

        TEST_CASE("EngineContext.ActivateOverlay delegates to OverlayStack")
        {
            IntegOverlay overlay;
            OverlayStack stack;
            stack.AddOverlay(&overlay, std::type_index(typeid(IntegOverlay)), OverlayDescriptor{.DefaultActive = false}, 0);

            EngineContext ctx;
            ctx.SetOverlayStack(&stack);

            // Initially inactive due to DefaultActive = false.
            CHECK_FALSE(stack.GetEntries()[0].ManuallyActive);

            ctx.ActivateOverlay(std::type_index(typeid(IntegOverlay)));
            CHECK(stack.GetEntries()[0].ManuallyActive);
        }

        TEST_CASE("EngineContext.DeactivateOverlay delegates to OverlayStack")
        {
            IntegOverlay overlay;
            OverlayStack stack;
            stack.AddOverlay(&overlay, std::type_index(typeid(IntegOverlay)), OverlayDescriptor{.DefaultActive = true}, 0);

            EngineContext ctx;
            ctx.SetOverlayStack(&stack);

            // Initially active.
            CHECK(stack.GetEntries()[0].ManuallyActive);

            ctx.DeactivateOverlay(std::type_index(typeid(IntegOverlay)));
            CHECK_FALSE(stack.GetEntries()[0].ManuallyActive);
        }

        TEST_CASE("State transition triggers capability recompute on OverlayStack")
        {
            // Setup tag registry so capability tags are valid.
            TagRegistry registry;
            NativeTag::RegisterAll(registry);

            // State A provides Rendering capability.
            CapabilitySet capsA;
            capsA.AddTag(Capability::Rendering);

            // State B provides no capabilities.
            CapabilitySet capsB;

            ApplicationStateMachine asm_;
            asm_.AddState<IntegStateA>(std::move(capsA));
            asm_.AddState<IntegStateB>(std::move(capsB));
            asm_.SetInitialState<IntegStateA>();
            asm_.AddTransition<IntegStateA, IntegStateB>();
            auto finaliseResult = asm_.Finalise();
            REQUIRE(finaliseResult.has_value());

            // Overlay requires Rendering.
            IntegOverlay overlay;
            OverlayStack overlayStack;
            OverlayDescriptor overlayDesc;
            overlayDesc.RequiredCapabilities.AddTag(Capability::Rendering);
            overlayStack.AddOverlay(&overlay, std::type_index(typeid(IntegOverlay)), std::move(overlayDesc), 0);

            EngineContext ctx;
            ctx.SetStateMachine(&asm_);
            ctx.SetOverlayStack(&overlayStack);

            // Start state A (which has Rendering) and update capabilities.
            asm_.Start(ctx);
            const auto* stateACaps = asm_.GetStateCapabilities(std::type_index(typeid(IntegStateA)));
            REQUIRE(stateACaps != nullptr);
            overlayStack.UpdateCapabilities(*stateACaps, ctx);
            CHECK(overlayStack.GetEntries()[0].CapabilitySatisfied);
            CHECK(overlay.Attached);

            // Transition to state B (no Rendering).
            ctx.RequestTransition<IntegStateB>();
            asm_.ProcessPending(ctx);
            const auto* stateBCaps = asm_.GetStateCapabilities(std::type_index(typeid(IntegStateB)));
            REQUIRE(stateBCaps != nullptr);
            overlayStack.UpdateCapabilities(*stateBCaps, ctx);

            CHECK_FALSE(overlayStack.GetEntries()[0].CapabilitySatisfied);
            CHECK(overlay.Detached);
        }

        TEST_CASE("IStateUI attaches on state enter and detaches on state exit")
        {
            s_lastStateUI = nullptr;

            ApplicationStateMachine asm_;
            asm_.AddState<IntegStateA>();
            asm_.AddState<IntegStateB>();
            asm_.SetInitialState<IntegStateA>();
            asm_.AddTransition<IntegStateA, IntegStateB>();
            asm_.RegisterStateUI(std::type_index(typeid(IntegStateA)), []() -> std::unique_ptr<IStateUI>
            {
                auto ui = std::make_unique<IntegStateUI>();
                s_lastStateUI = ui.get();
                return ui;
            });
            auto finaliseResult = asm_.Finalise();
            REQUIRE(finaliseResult.has_value());

            EngineContext ctx;
            ctx.SetStateMachine(&asm_);

            // Start enters state A, which should attach its UI.
            asm_.Start(ctx);
            REQUIRE(s_lastStateUI != nullptr);
            CHECK(s_lastStateUI->Attached);

            auto* uiPtr = s_lastStateUI;

            // Transition to B exits A, which should detach the UI.
            ctx.RequestTransition<IntegStateB>();
            asm_.ProcessPending(ctx);
            CHECK(uiPtr->Detached);
        }

        TEST_CASE("IStateUI suspends on push and resumes on pop")
        {
            s_lastStateUI = nullptr;

            ApplicationStateMachine asm_;
            asm_.AddState<IntegStateA>();
            asm_.AddState<IntegStateB>();
            asm_.SetInitialState<IntegStateA>();
            asm_.AllowPush<IntegStateB>();
            asm_.RegisterStateUI(std::type_index(typeid(IntegStateA)), []() -> std::unique_ptr<IStateUI>
            {
                auto ui = std::make_unique<IntegStateUI>();
                s_lastStateUI = ui.get();
                return ui;
            });
            auto finaliseResult = asm_.Finalise();
            REQUIRE(finaliseResult.has_value());

            EngineContext ctx;
            ctx.SetStateMachine(&asm_);
            asm_.Start(ctx);
            REQUIRE(s_lastStateUI != nullptr);
            CHECK(s_lastStateUI->Attached);

            auto* uiPtr = s_lastStateUI;

            // Push B suspends A's UI.
            ctx.RequestPush<IntegStateB>();
            asm_.ProcessPending(ctx);
            CHECK(uiPtr->Suspended);

            // Pop B resumes A's UI.
            ctx.RequestPop();
            asm_.ProcessPending(ctx);
            CHECK(uiPtr->Resumed);
        }

        TEST_CASE("Lifecycle hooks fire during transitions through EngineContext")
        {
            bool hookFired = false;

            LifecycleHookRegistrar registrar;
            registrar.OnStateEnter<IntegStateB>([&](EngineContext&)
            {
                hookFired = true;
            });
            auto hookResult = registrar.Finalise();
            REQUIRE(hookResult.has_value());

            ApplicationStateMachine asm_;
            asm_.AddState<IntegStateA>();
            asm_.AddState<IntegStateB>();
            asm_.SetInitialState<IntegStateA>();
            asm_.AddTransition<IntegStateA, IntegStateB>();
            asm_.SetLifecycleHooks(&(*hookResult));
            auto finaliseResult = asm_.Finalise();
            REQUIRE(finaliseResult.has_value());

            EngineContext ctx;
            ctx.SetStateMachine(&asm_);
            asm_.Start(ctx);

            ctx.RequestTransition<IntegStateB>();
            asm_.ProcessPending(ctx);

            CHECK(hookFired);
        }

        TEST_CASE("Full cascade: AppBuilder -> ASM + OverlayStack -> EngineContext")
        {
            // Verify end-to-end flow from AppBuilder registration through to EngineContext delegation.
            TagRegistry registry;
            NativeTag::RegisterAll(registry);

            AppBuilder builder;
            CapabilitySet capsA;
            capsA.AddTag(Capability::Rendering);
            builder.AddState<IntegStateA>(std::move(capsA));
            builder.AddState<IntegStateB>();
            builder.SetInitialState<IntegStateA>();
            builder.AddTransition<IntegStateA, IntegStateB>();
            builder.RegisterOverlay<IntegOverlay>();
            auto descResult = builder.Finalise();
            REQUIRE(descResult.has_value());

            // Verify manifests were produced.
            CHECK(descResult->Has<StateManifest>());
            CHECK(descResult->Has<OverlayManifest>());

            // Construct ASM using same state types (verifying manifest data is correct).
            const auto& stateManifest = descResult->Get<StateManifest>();
            CHECK(stateManifest.InitialState == std::type_index(typeid(IntegStateA)));
            CHECK(stateManifest.States.size() == 2);

            ApplicationStateMachine asm_;
            asm_.AddState<IntegStateA>();
            asm_.AddState<IntegStateB>();
            asm_.SetInitialState<IntegStateA>();
            asm_.AddTransition<IntegStateA, IntegStateB>();
            auto asmResult = asm_.Finalise();
            REQUIRE(asmResult.has_value());

            // Wire EngineContext.
            EngineContext ctx;
            ctx.SetStateMachine(&asm_);
            asm_.Start(ctx);

            CHECK(asm_.GetActiveStateType() == std::type_index(typeid(IntegStateA)));

            // Delegate a transition through EngineContext.
            ctx.RequestTransition<IntegStateB>();
            asm_.ProcessPending(ctx);
            CHECK(asm_.GetActiveStateType() == std::type_index(typeid(IntegStateB)));
        }
    }

} // namespace Wayfinder::Tests
