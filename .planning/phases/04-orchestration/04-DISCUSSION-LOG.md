# Phase 4: Orchestration - Discussion Log

**Session:** 2026-04-06
**Gray areas discussed:** 5 of 5 selected

---

## Gray Area 1: ApplicationStateMachine Design

### Question
How is the ASM keyed, who owns states, and how are states registered?

### Approaches Considered

**Keying:**
1. `std::type_index` -- one instance per type, compile-time safety
2. `InternedString` -- data-driven, runtime-definable
3. `enum class` -- like generic StateMachine<TStateId>

**Ownership:**
1. ASM owns via `unique_ptr`
2. AppDescriptor owns, ASM borrows
3. States self-register (static)

**Registration:**
1. Build-time descriptor via AppBuilder + Finalise() validation
2. Runtime registration (add states dynamically)
3. Self-registration (static constructors)

### Engine Research
- **Oxylus:** App owns module registry, deferred task queue
- **RavEngine:** World-based state management, worlds as states
- **Bevy:** SubStates derived from parent states, computed states

### Discussion

**Keying:** User confirmed `std::type_index` immediately. One instance per state type, `TransitionTo<T>()` API, mirrors SubsystemRegistry.

**Ownership:** User asked for real-world examples of all 3 approaches. Provided analysis showing:
- ASM-owns: clearest lifetime, states as lightweight shells with heavy data in subsystems
- AppDescriptor-owns: creates split ownership between descriptor (holds) and ASM (manages lifecycle)
- Self-register: global state, construction order issues
User chose ASM owns.

**Registration:** User asked about editor implications. Provided PIE (Play-In-Editor) analysis showing two models:
- Simulate: editor stays active, simulation runs in same world
- Full Play: push GameplayState over EditorState
Both handled by push/pop + capability system without special infrastructure. User confirmed build-time descriptor registration.

### Decisions
- D-01: `std::type_index` keying
- D-02: ASM owns via `unique_ptr`
- D-03: Build-time registration, Finalise() validation

---

## Gray Area 2: Push/Pop Negotiation Model

### Question
How do states negotiate what happens during push/pop (background behaviour, subsystem persistence)?

### Approaches Considered

**Negotiation:**
1. One-sided: foreground decides everything
2. Two-sided: BackgroundPreferences + SuspensionPolicy, AND intersection
3. Static policy: fixed per-state, no negotiation

**Subsystem persistence:**
1. Always persist (no teardown during push/pop)
2. Selective teardown (configurable per-subsystem)

### Discussion

**Negotiation:** User asked which model is best. Provided network multiplayer example proving two-sided is necessary: GameplayState as background wants to keep network subsystem alive (BackgroundPreferences.KeepSubsystems); PauseState as foreground must allow it (SuspensionPolicy.AllowSubsystems). Neither side alone has complete information. AND intersection gives both sides a veto. User confirmed.

**Subsystem persistence:** Initially presented examples using CutsceneState, InventoryState -- user correctly pointed out these are sub-states within GameplayState, not application-level states. Corrected: application-level push/pop is for coarse modal states (PauseState over GameplayState). There are maybe 2-3 pushable states in a typical game. No scenario exists where you'd tear down GameplayState's subsystems for a pause menu. Always persist is the obvious answer.

### Key Insight
Application-level push/pop is rare and coarse. CutsceneState/InventoryState are game-level sub-states within the per-state StateMachine, not ApplicationStateMachine states. This distinction is critical for Phase 4 scoping.

### Decisions
- D-04: Two-sided negotiation (BackgroundPreferences + SuspensionPolicy, AND intersection)
- D-05: Always persist state subsystems during push/pop

---

## Gray Area 3: OverlayStack Execution Model

### Question
How are overlays owned, how does input propagation work, and how is execution order determined?

### Approaches Considered

**Ownership:**
1. AppBuilder registration, Application owns, OverlayStack is non-owning view
2. OverlayStack owns everything
3. Plugins create at runtime

**Input:**
1. `bool OnEvent()` consumed flag
2. Always propagate
3. Focus system

**Ordering:**
1. Registration order = Z-order
2. Explicit priority values
3. Layer grouping

### Discussion

**Ownership:** User asked if there are reasons to consider alternatives. Analysis showed:
- OverlayStack-owns is functionally identical but couples ownership to execution layer
- Runtime creation solves a problem that barely exists (engine overlays are known at build time; runtime overlay addition can be added later as `EngineContext::AttachOverlay()`)
User confirmed AppBuilder registration.

**Input:** User asked about a shared UI toolkit focus system for overlays AND IStateUI. Analysis showed:
- Focus system is the correct long-term answer, but no UI toolkit exists yet
- `bool OnEvent()` is the primitive mechanism that a focus system builds ON TOP OF (focus = routing policy, consumed-bool = mechanism)
- They don't compete -- consumed-event is the low-level contract, focus is the high-level policy
- ImGui already handles its own focus via `io.WantCaptureKeyboard`
User asked about `Result` vs `bool`. Analysis showed errors in event handling are overlay-internal (log and recover) -- `Result` adds ceremony without value because there's no meaningful caller response to an event-handling failure at 60fps. User confirmed `bool`.

**Ordering:** User requested real-world examples of all 3 approaches. Provided:
- Registration order: Oxylus, Spartan, WickedEngine (ImGui-based, hardcoded order). Issue: plugin load order becomes visible behaviour.
- Priority values: Unity Script Execution Order, O3DE. Issue: magic numbers, coordination burden.
- Layer grouping: Unreal Gameplay Layers, Godot CanvasLayer. Issue: over-engineered for <10 overlays.
Recommendation: registration order + optional priority override. User confirmed.

### Decisions
- D-06: AppBuilder registration, Application owns, OverlayStack is non-owning view
- D-07: `bool OnEvent()` for consumption (focus system builds on top later)
- D-08: Registration order + optional `int32_t Priority` override

---

## Gray Area 4: Simulation Replacing Game

### Question
What does Simulation own, when does it exist, and how does game code access it?

### Approaches Considered

**Scope:**
1. Thin: ECS world + Scene only
2. Fat: like Game but composable
3. Minimal: just ECS world

**Lifecycle:**
1. Per-state, created in OnEnter/OnExit
2. Build-time registered, capability-activated
3. Global singleton

**Access:**
1. Simulation IS a state subsystem (via EngineContext)
2. Simulation owns ServiceProvider (game code queries simulation)
3. Unified adapter on EngineContext (searches multiple sources)

### Discussion

**Scope and lifecycle:** User quickly confirmed thin (world + scene) and per-state. Straightforward.

**Access:** User asked for pros/cons of all 3 with real examples and future engine considerations. Provided detailed analysis:
- Option A (state subsystem): consistent, no new patterns, subsystem graph handles ordering. Con: verbose access (`context.GetStateSubsystem<Simulation>().GetWorld()`) but cacheable.
- Option B (owns ServiceProvider): tailored interface but dual access patterns, chicken-and-egg problem with subsystem initialization.
- Option C (unified EngineContext adapter): clean API but service locator anti-pattern, lifetime ambiguity, god object.
Real-world references: O3DE system entity (Option A), Unity VContainer (Option B), ASP.NET IServiceProvider (Option C).

User chose Option A but asked about reversibility. Confirmed: extraction to a plain class is mechanical because subsystems cache `Simulation*` in OnInitialise -- only the init methods change, not the usage sites.

### Decisions
- D-09: Thin scope (ECS world + Scene only)
- D-10: Per-state lifecycle
- D-11: Simulation IS a state subsystem (reversible)

---

## Gray Area 5: Sub-state Machines & IStateUI Registration

### Question
Who owns sub-state machines, how is IStateUI paired with states, and who manages IStateUI lifecycle?

### Approaches Considered

**Sub-state ownership:**
1. State-internal (state creates its own StateMachine<TStateId>)
2. AppBuilder declarative (`builder.ForState<T>().RegisterSubState<Combat>()`)
3. Base class (`SubStatefulApplicationState<TStateId>`)

**IStateUI pairing:**
1. One IStateUI per ApplicationState
2. IStateUI per sub-state
3. Independent registration, state controls

**IStateUI lifecycle:**
1. ASM manages automatically
2. State manages manually

### Discussion

**Sub-states:** User leaned state-internal, asked if alternatives make sense. Analysis showed:
- AppBuilder declarative couples engine to game-specific state enums (CombatState, CutsceneState are game concepts), violates "engine is a library"
- Base class contradicts composition-over-inheritance, forces pattern on states that don't need sub-states
User confirmed state-internal. **Note:** This refines SIM-06 requirement text which says `builder.ForState<T>().RegisterSubState()`.

**IStateUI:** User said they couldn't imagine Options 2 or 3. Analysis showed both collapse:
- Per-sub-state requires the engine to observe sub-state transitions, but sub-states are state-internal (engine can't observe what it doesn't know about). Per-sub-state UI is handled by IStateUI internally swapping content.
- Independent registration removes IStateUI's defining characteristic (state-bound lifecycle). That's just overlays with extra steps.
User confirmed one IStateUI per ApplicationState.

**Lifecycle:** User immediately confirmed ASM manages automatically.

### Decisions
- D-12: Sub-states are state-internal (requirement SIM-06 refined -- engine provides StateMachine<TStateId> tool but doesn't manage sub-state registration)
- D-13: One IStateUI per ApplicationState via `builder.ForState<T>().SetUI<U>()`
- D-14: ASM manages IStateUI lifecycle automatically
