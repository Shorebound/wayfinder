# Phase 4: Orchestration - Context

**Gathered:** 2026-04-06
**Status:** Ready for planning

<domain>
## Phase Boundary

ApplicationStateMachine, Simulation, and OverlayStack operate together with full lifecycle control and state transition management. This phase delivers: ApplicationStateMachine with type_index-keyed states, unique_ptr ownership, flat transitions (replace) and push/pop modal stack with deferred execution at frame boundaries; two-sided push/pop negotiation (BackgroundPreferences + SuspensionPolicy AND intersection); Simulation as a thin state subsystem (flecs world + Scene only) replacing Game; OverlayStack with registration-order execution (input top-down, render bottom-up), capability-gated activation, runtime toggle; IStateUI registration per ApplicationState with ASM-managed lifecycle mirroring; sub-state machines as state-internal (state creates its own StateMachine<TStateId>); EngineContextServiceProvider adapter; ActiveGameState singleton updated via transition callbacks.

**Requirements:** STATE-02, STATE-03, STATE-04, STATE-05, STATE-08, SIM-01, SIM-03, SIM-06, SIM-07, OVER-02, OVER-03, OVER-04, UI-02, UI-03

</domain>

<decisions>
## Implementation Decisions

### ApplicationStateMachine Design
- **D-01:** ASM keyed by `std::type_index`. One instance per state type, compile-time safety via `TransitionTo<T>()` / `Push<T>()`. Mirrors SubsystemRegistry keying. Avoids string/enum disambiguation.
- **D-02:** ASM owns states via `unique_ptr<IApplicationState>`. States are lightweight shells -- heavy data lives in state subsystems. Lifetime ownership is unambiguous. States created during Finalise() from registered descriptors.
- **D-03:** Build-time descriptor registration via AppBuilder (e.g. `builder.AddState<GameplayState>()`). All states and transitions declared before `Finalise()`. Finalise validates the complete state graph (reachability, declared transitions only). No runtime state addition. Consistent with Phase 1 StateMachine pattern and Phase 3 AppBuilder flow.

### Push/Pop Negotiation
- **D-04:** Two-sided negotiation via a `BackgroundMode` flags enum (`None`, `Update`, `Render`, `All`). Each IApplicationState returns two `BackgroundMode` values: `GetBackgroundPreferences()` (what it wants when suspended) and `GetSuspensionPolicy()` (what it allows the background state to do when it's the foreground). Effective policy = bitwise AND of the two. Defaults: preferences = `None` (conservative), policy = `Render` (pause-over-gameplay shows the game behind). Prefer this single `enum class` with bitwise ops over separate bool structs -- matches the codebase idiom and the intersection is a single `&`.
- **D-05:** State subsystems always persist during push/pop at the application level. No selective teardown. Application-level push/pop is for coarse modal states (PauseState over GameplayState) where teardown makes no sense. There are maybe 2-3 pushable states in a typical game. Simplicity wins over edge-case flexibility.

### OverlayStack Execution Model
- **D-06:** Overlays registered via AppBuilder (`builder.RegisterOverlay<DebugOverlay>()`). Application owns `vector<unique_ptr<IOverlay>>`. OverlayStack is a non-owning execution view over the owned overlays. Consistent with subsystem/state registration pattern. Validated at Finalise().
- **D-07:** `IOverlay::OnEvent` returns `bool` (consumed/not-consumed). If true, event stops propagating to lower overlays and the active state. Top-down order means highest overlay gets first shot. This is the primitive mechanism that a future UI toolkit focus system builds *on top of* as the routing policy. Errors in event handling are overlay-internal (log via `Wayfinder::Log`, recover or disable self).
- **D-08:** Registration order determines execution order (Z-order). Optional `int32_t Priority` override on registration for cases where plugin ordering creates problems. Default priority = registration order index. No layer taxonomy -- under 10 overlays don't justify the abstraction.

### Simulation Replacing Game
- **D-09:** Simulation is thin: owns flecs::world + Scene only. Physics, audio, and other game subsystems are registered as state subsystems separately. Simulation is a service those subsystems consume, not a container for them.
- **D-10:** Simulation is per-state, created during the state's lifecycle (OnEnter creates, OnExit destroys). States that don't need simulation (MenuState) don't create one. Matches IApplicationState lifecycle.
- **D-11:** Simulation IS a state subsystem. Accessed via `context.GetStateSubsystem<Simulation>()`. No new access pattern. Subsystem dependency graph handles init ordering (Simulation before Physics) for free. Capability-gating works naturally. Subsystems that use Simulation cache the pointer in OnInitialise -- the EngineContext lookup is not per-frame. If this proves wrong, extraction to a plain class is mechanical (change init method signatures, not usage sites). Deliberately reversible.
- **D-12 (requirement refinement):** SIM-06 requirement text says `builder.ForState<T>().RegisterSubState()`. Discussion concluded sub-states are state-internal: the ApplicationState creates its own `StateMachine<TStateId>` in OnEnter. The engine provides the tool (generic StateMachine from Phase 1) but does not mandate or manage sub-state usage. Rationale: sub-states are game design decisions (CombatState, CutsceneState are game concepts), not engine architecture. AppBuilder declarative registration would couple the engine to game-specific state enums, violating "engine is a library." IStateUI registration remains via builder (D-13) because IStateUI lifecycle is engine-managed.

### Sub-state Machines & IStateUI
- **D-13:** One IStateUI per ApplicationState. Registered via `builder.ForState<T>().SetUI<GameplayUI>()`. IStateUI's lifecycle mirrors the owning ApplicationState: OnAttach on state enter, OnDetach on state exit, OnSuspend/OnResume on push/pop. Per-sub-state UI is handled by the IStateUI internally (it swaps content based on the sub-state), not by the engine.
- **D-14:** ASM manages IStateUI lifecycle automatically. No manual lifecycle calls needed in the ApplicationState. ApplicationStateMachine calls IStateUI::OnAttach when entering a state, OnDetach when exiting, OnSuspend/OnResume matching the state's own suspend/resume on push/pop.

### Editor & PIE Considerations (noted, not Phase 4 scope)
- **D-15 (noted):** PIE (Play-In-Editor) has two models: Simulate (editor state stays active, simulation runs in same world) and Full Play (push GameplayState over EditorState). Both handled by push/pop + capability system. EditorState retains Editing capability in background; GameplayState adds Simulation. No special PIE infrastructure needed -- it falls out of the architecture.

### Agent's Discretion
- Internal storage for ASM (e.g. `std::unordered_map<std::type_index, unique_ptr<IApplicationState>>` or flat vector with type_index lookup)
- BackgroundPreferences / SuspensionPolicy struct field names and defaults (the AND intersection semantic is fixed, the struct layout is flexible)
- OverlayStack internal data structure (vector of non-owning pointers, sorted by effective priority)
- How `ActiveGameState` singleton update is wired (direct callback vs lifecycle hook vs ASM observer)
- EngineContextServiceProvider adapter implementation details
- How `builder.ForState<T>().SetUI<U>()` stores the type-erased UI factory in AppDescriptor
- Whether Simulation exposes World/Scene via getters or a more structured API
- Frame boundary queueing mechanism for deferred transitions (single pending vs queue)
- How capability-gated overlay activation is checked (OverlayStack queries CapabilitySet directly, or filtered on state transition)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Architecture Specifications
- `docs/plans/application_architecture_v2.md` -- ApplicationStateMachine design, push/pop negotiation (BackgroundPreferences + SuspensionPolicy), OverlayStack execution model, startup lifecycle sequence
- `docs/plans/application_migration_v2.md` -- Game -> Simulation migration path, ApplicationStateMachine integration point, EngineRuntime decomposition dependencies
- `docs/plans/game_framework.md` -- Simulation design, ServiceProvider, state interaction with plugins, ActiveGameState, sub-state machines, IStateUI lifecycle

### Existing Types (to build on or evolve)
- `engine/wayfinder/src/gameplay/StateMachine.h` -- Phase 1 generic StateMachine<TStateId, THash>. Study for pattern reference (Finalise/Start/TransitionTo/ProcessPending, BFS reachability). ApplicationStateMachine will be a SEPARATE implementation using type_index keying, not a specialisation of this template.
- `engine/wayfinder/src/app/IApplicationState.h` -- Phase 1 interface. OnEnter/OnExit (Result<void>), OnSuspend/OnResume/OnUpdate/OnRender/OnEvent (void). Phase 4 adds BackgroundPreferences/SuspensionPolicy virtual methods.
- `engine/wayfinder/src/app/IOverlay.h` -- Phase 1 interface. OnAttach/OnDetach (Result<void>), OnUpdate/OnRender/OnEvent (void). Phase 4 changes OnEvent return to bool for consumption.
- `engine/wayfinder/src/plugins/IStateUI.h` -- Phase 1 interface. Full lifecycle mirroring state. Phase 4 wires to ASM-managed automatic lifecycle.
- `engine/wayfinder/src/app/EngineContext.h` -- Phase 2 output with Phase 4 assert stubs: RequestTransition<T>(), RequestPush<T>(), RequestPop(), ActivateOverlay(), DeactivateOverlay(). Phase 4 implements these.
- `engine/wayfinder/src/app/SubsystemRegistry.h` + `SubsystemManifest.h` -- Phase 2 output, retrofitted in Phase 3. Simulation IS a state subsystem registered through this.
- `engine/wayfinder/src/app/AppBuilder.h` + `AppBuilder.cpp` -- Phase 3 output. AddState, RegisterStateSubsystem, ForState pattern. Phase 4 adds RegisterOverlay, ForState<T>().SetUI<U>().
- `engine/wayfinder/src/app/AppDescriptor.h` -- Phase 3 output. Type-erased processed outputs. Phase 4 stores OverlayManifest, StateUIManifest, StateGraph.
- `engine/wayfinder/src/app/Application.h` + `Application.cpp` -- Mixed v1/v2. Phase 4 creates ApplicationStateMachine and OverlayStack as new owned members, wires EngineContext stubs to real implementations.
- `engine/wayfinder/src/gameplay/Game.h` + `Game.cpp` -- Current runtime owning flecs::world, subsystem collection, scene, assets, game state machine. Simulation REPLACES this. Study for what needs migrating.
- `engine/wayfinder/src/app/Capability.h` -- CapabilitySet = TagContainer. Well-known: Simulation, Rendering, Presentation, Editing. Used for overlay capability-gating.
- `engine/wayfinder/src/core/ServiceProvider.h` -- Concept + StandaloneServiceProvider. EngineContextServiceProvider adapter wraps EngineContext to satisfy this concept.
- `engine/wayfinder/src/app/LifecycleHooks.h` -- Phase 3 output. OnStateEnter<T> / OnStateExit<T> hooks. ASM fires these during transitions.

### Prior Phase Contexts
- `.planning/phases/01-foundation-types/` -- StateMachine, IApplicationState, IOverlay, IStateUI, Capability foundations
- `.planning/phases/02-subsystem-infrastructure/` -- SubsystemRegistry, EngineContext, SubsystemManifest, capability-gated activation
- `.planning/phases/03-plugin-composition/` -- AppBuilder, AppDescriptor, LifecycleHooks, ConfigService, processed-output pattern

</canonical_refs>
