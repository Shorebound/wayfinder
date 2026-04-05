# Phase 4: Orchestration - Research

**Researched:** 2026-04-06
**Domain:** Application state machine, simulation lifecycle, overlay execution, C++23 orchestration patterns
**Confidence:** HIGH

## Summary

Phase 4 is the largest and most architecturally significant phase of the migration. It delivers three tightly coupled runtime systems -- ApplicationStateMachine, OverlayStack, and Simulation -- that together replace the current monolithic `Game`/`LayerStack` orchestration. All three systems share the `EngineContext` facade, follow the build-time registration / Finalise() / runtime execution pattern established in Phases 2-3, and must interoperate under a single deferred frame-boundary processing model.

The primary technical challenge is not any individual system -- each is well-understood in isolation -- but the interaction surface: state transitions must trigger capability recomputation, which gates overlay activation, which affects event routing, which flows through the state and its IStateUI. The implementation must handle this cascade in a deterministic, single-pass order at frame boundaries.

C++23 provides strong tools for this phase: `std::type_index` for type-safe keying (already used in SubsystemRegistry), `std::expected` (already wrapped as `Result<T>`), concepts for constraining template parameters, and deducing `this` for cleaner accessor patterns. The existing codebase patterns (registrar -> Finalise() -> manifest, `unique_ptr` ownership, `WAYFINDER_ASSERT` for preconditions) should be followed precisely.

**Primary recommendation:** Build each system independently (ASM, then Simulation, then OverlayStack) with unit tests, then wire them together in EngineContext. The transition cascade (state change -> capability recompute -> overlay activate/deactivate -> fire lifecycle hooks -> IStateUI lifecycle) should be a single well-defined sequence in ApplicationStateMachine::ProcessPending(), not distributed across systems.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** ASM keyed by `std::type_index`. One instance per state type, compile-time safety via `TransitionTo<T>()` / `Push<T>()`. Mirrors SubsystemRegistry keying. Avoids string/enum disambiguation.
- **D-02:** ASM owns states via `unique_ptr<IApplicationState>`. States are lightweight shells -- heavy data lives in state subsystems. Lifetime ownership is unambiguous. States created during Finalise() from registered descriptors.
- **D-03:** Build-time descriptor registration via AppBuilder (e.g. `builder.AddState<GameplayState>()`). All states and transitions declared before `Finalise()`. Finalise validates the complete state graph (reachability, declared transitions only). No runtime state addition. Consistent with Phase 1 StateMachine pattern and Phase 3 AppBuilder flow.
- **D-04:** Two-sided negotiation model. `BackgroundPreferences` (what the background state wants: keep updating, keep rendering, keep specific subsystems) AND `SuspensionPolicy` (what the foreground state allows: allow background update, allow background render, allow specific subsystems). Effective policy = AND intersection. Both are plain structs returned from virtual methods on IApplicationState.
- **D-05:** State subsystems always persist during push/pop at the application level. No selective teardown.
- **D-06:** Overlays registered via AppBuilder (`builder.RegisterOverlay<DebugOverlay>()`). Application owns `vector<unique_ptr<IOverlay>>`. OverlayStack is a non-owning execution view over the owned overlays. Consistent with subsystem/state registration pattern. Validated at Finalise().
- **D-07:** `IOverlay::OnEvent` returns `bool` (consumed/not-consumed). If true, event stops propagating to lower overlays and the active state. Top-down order means highest overlay gets first shot.
- **D-08:** Registration order determines execution order (Z-order). Optional `int32_t Priority` override on registration for cases where plugin ordering creates problems. Default priority = registration order index. No layer taxonomy.
- **D-09:** Simulation is thin: owns flecs::world + Scene only. Physics, audio, and other game subsystems are registered as state subsystems separately.
- **D-10:** Simulation is per-state, created during the state's lifecycle (OnEnter creates, OnExit destroys).
- **D-11:** Simulation IS a state subsystem. Accessed via `context.GetStateSubsystem<Simulation>()`. Subsystem dependency graph handles init ordering.
- **D-12:** Sub-states are state-internal: the ApplicationState creates its own `StateMachine<TStateId>` in OnEnter. The engine provides the tool (generic StateMachine from Phase 1) but does not mandate or manage sub-state usage.
- **D-13:** One IStateUI per ApplicationState. Registered via `builder.ForState<T>().SetUI<GameplayUI>()`. IStateUI's lifecycle mirrors the owning ApplicationState.
- **D-14:** ASM manages IStateUI lifecycle automatically. No manual lifecycle calls needed in the ApplicationState.
- **D-15 (noted, not Phase 4 scope):** PIE falls out of push/pop + capability system.

### Agent's Discretion
- Internal storage for ASM (e.g. `std::unordered_map<std::type_index, unique_ptr<IApplicationState>>` or flat vector with type_index lookup)
- BackgroundPreferences / SuspensionPolicy struct field names and defaults
- OverlayStack internal data structure
- How `ActiveGameState` singleton update is wired
- EngineContextServiceProvider adapter implementation details
- How `builder.ForState<T>().SetUI<U>()` stores the type-erased UI factory in AppDescriptor
- Whether Simulation exposes World/Scene via getters or a more structured API
- Frame boundary queueing mechanism for deferred transitions (single pending vs queue)
- How capability-gated overlay activation is checked

### Deferred Ideas (OUT OF SCOPE)
- D-15 PIE (Play-In-Editor) implementation
- Multi-IStateUI per state
- Tag-gated widget activation
- Visual UI designer tooling
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| STATE-02 | ApplicationStateMachine with flat transitions (replace) and push/pop modal stack | ASM architecture patterns, type_index keying, stack design |
| STATE-03 | Deferred state transitions processed at frame boundaries | Frame boundary queueing patterns, single-pending model |
| STATE-04 | State transition validation (declared transitions only, startup error on undeclared) | Build-time graph validation, BFS reachability (extends Phase 1 StateMachine pattern) |
| STATE-05 | Push/pop negotiation (BackgroundPreferences + SuspensionPolicy intersection) | Negotiation struct design, AND intersection semantics |
| STATE-08 | State subsystems persist across push/pop (no teardown for modal states) | Push/pop lifecycle: OnSuspend/OnResume only, no Shutdown/Initialise cycle |
| SIM-01 | Simulation class replacing Game (flecs world + scene management only) | Thin world wrapper pattern, Game -> Simulation migration |
| SIM-03 | EngineContextServiceProvider adapter for live usage | ServiceProvider concept adapter wrapping EngineContext |
| SIM-06 | Sub-state registration - REFINED: sub-states are state-internal (D-12) | StateMachine<TStateId> as member variable pattern |
| SIM-07 | ActiveGameState singleton updated via transition callbacks | ECS singleton pattern, OnTransition callback wiring |
| OVER-02 | OverlayStack with registration-order execution (input top-down, render bottom-up) | Overlay execution model, priority-sorted non-owning view |
| OVER-03 | Capability-gated activation/deactivation on state transitions | CapabilitySet satisfaction check during transition cascade |
| OVER-04 | Runtime toggle (ActivateOverlay/DeactivateOverlay) | Manual activation via EngineContext, deferred or immediate |
| UI-02 | Registration via builder.ForState<T>().SetUI<U>() | Type-erased factory in AppDescriptor, ForState pattern |
| UI-03 | Lifecycle mirroring state (attach/detach/suspend/resume/update/render/event) | ASM-managed automatic lifecycle forwarding |
</phase_requirements>

## Project Constraints (from copilot-instructions.md)

Key constraints that affect Phase 4 implementation:

- **C++23**: Use `std::expected` (via `Result<T>`), concepts, deducing `this`, `constexpr`/`consteval`, trailing return types
- **RAII everywhere**: `unique_ptr` ownership for states, overlays. No raw `new`/`delete`
- **Result<T> for recoverable failures**: OnEnter/OnExit return `Result<void>`, not bool or exceptions
- **`[[nodiscard]]`** on functions returning resources or values the caller must inspect
- **`and`/`or`/`not`** over `&&`/`||`/`!`
- **West const**: `const T`, not `T const`
- **Trailing return types**: `auto Foo() -> ReturnType`
- **`Wayfinder::Log` category logging**: Use `LogEngine` category for ASM/OverlayStack lifecycle logging
- **British spelling**: `Initialise`, `Finalise`, etc.
- **Explicit source file listing**: New `.cpp`/`.h` files must be added to `CMakeLists.txt`
- **Doctest for testing**: Headless, no GPU, fixtures in `tests/fixtures/`
- **`InternedString` for stable identifiers**: Already used for tag names, state IDs in sub-state machines
- **Namespace**: Engine code in `Wayfinder`, no sub-namespace needed for app-level types

## Architecture Patterns

### Pattern 1: ApplicationStateMachine Internal Architecture

**What:** A type_index-keyed state machine with flat transitions, push/pop modal stack, and deferred execution.

**Storage recommendation:** `std::unordered_map<std::type_index, std::unique_ptr<IApplicationState>>` for the state registry (mirrors SubsystemRegistry pattern). A `std::vector<std::type_index>` for the modal stack (active state at back). The map provides O(1) lookup for transitions; the vector provides ordered push/pop with clear ownership semantics.

**Transition graph:** Store as `std::unordered_map<std::type_index, std::unordered_set<std::type_index>>` -- each state maps to its set of allowed transition targets. Validated at Finalise() time via BFS reachability from the initial state, identical to the Phase 1 StateMachine pattern.

**Deferred transitions:** Use a `std::variant<std::monostate, FlatTransition, PushTransition, PopTransition>` for the single pending operation. Only one operation per frame -- queueing a second overwrites with a warning log. This is simpler than a queue and matches real usage (a state decides ONE thing per frame). The three operation types:

```cpp
struct FlatTransition { std::type_index Target; };
struct PushTransition { std::type_index Target; };
struct PopTransition {};

using PendingOperation = std::variant<std::monostate, FlatTransition, PushTransition, PopTransition>;
```

**ProcessPending cascade order (critical):**
1. Extract pending operation, reset to monostate
2. Execute operation (OnExit current / OnSuspend current / OnEnter new / OnResume resumed)
3. Recompute effective capabilities (app caps + new state caps)
4. Reactivate/deactivate overlays based on new capabilities
5. Fire LifecycleHookManifest::FireStateEnter/FireStateExit hooks
6. Update IStateUI lifecycle (OnAttach/OnDetach/OnSuspend/OnResume)
7. Update ActiveGameState ECS singleton if applicable

This is a single-pass, deterministic sequence. No system observes a partial transition.

**Example (verified against existing codebase patterns):**

```cpp
class ApplicationStateMachine
{
public:
    /// Build-time registration. Call before Finalise().
    template<std::derived_from<IApplicationState> T>
    void AddState(StateRegistrationDescriptor descriptor);

    /// Declare a valid flat transition. Call before Finalise().
    template<std::derived_from<IApplicationState> TFrom, std::derived_from<IApplicationState> TTo>
    void AddTransition();

    /// Validate graph and freeze. Creates state instances from factories.
    [[nodiscard]] auto Finalise(std::type_index initialState) -> Result<void>;

    /// Queue a flat transition (deferred to next ProcessPending).
    template<std::derived_from<IApplicationState> T>
    void RequestTransition();

    /// Queue a push (deferred to next ProcessPending).
    template<std::derived_from<IApplicationState> T>
    void RequestPush();

    /// Queue a pop (deferred to next ProcessPending).
    void RequestPop();

    /// Execute at frame boundary. Runs the full transition cascade.
    void ProcessPending(EngineContext& context);

    /// Access the active (topmost) state.
    [[nodiscard]] auto GetActiveState() -> IApplicationState*;

    /// Iterate the modal stack for background update/render.
    [[nodiscard]] auto GetModalStack() const -> std::span<const std::type_index>;

private:
    std::unordered_map<std::type_index, std::unique_ptr<IApplicationState>> m_states;
    std::unordered_map<std::type_index, std::unordered_set<std::type_index>> m_transitions;
    std::unordered_map<std::type_index, StateRegistrationDescriptor> m_descriptors;
    std::vector<std::type_index> m_stack;  // active state at back
    PendingOperation m_pending;
    std::type_index m_initialState{typeid(void)};
    bool m_finalised = false;
    bool m_running = false;
};
```

### Pattern 2: Push/Pop Negotiation Model

**What:** Two-sided agreement on what the background state does while suspended.

**Recommended approach: `BackgroundMode` flags enum.**

Collapses three bool structs into a single type. Intersection is bitwise AND -- no free function needed. Matches the codebase idiom of `enum class` with bitwise ops.

```cpp
enum class BackgroundMode : uint8_t
{
    None   = 0,
    Update = 1 << 0,
    Render = 1 << 1,
    All    = Update | Render,
};

// Bitwise operators (operator|, operator&, operator~, etc.)
```

IApplicationState provides two virtual methods returning the same type:

```cpp
// What the state wants when it's in the background (suspended)
virtual auto GetBackgroundPreferences() const -> BackgroundMode { return BackgroundMode::None; }

// What the state allows the background to do when it's the foreground
virtual auto GetSuspensionPolicy() const -> BackgroundMode { return BackgroundMode::Render; }
```

Defaults are conservative: background state neither updates nor renders unless both sides agree. `GetSuspensionPolicy` defaults to `Render` because the common case (pause menu over gameplay) wants the game visible behind the overlay.

**Intersection:** `auto effective = prefs & policy;`

**Usage in the frame loop:** After ProcessPending, iterate the stack from back to front. For each state below the top, compute `effective = background.GetBackgroundPreferences() & foreground.GetSuspensionPolicy()`. Call OnUpdate if `(effective & BackgroundMode::Update) != BackgroundMode::None`, OnRender likewise.

**Alternative (not recommended):** Separate `BackgroundPreferences` / `SuspensionPolicy` / `EffectiveBackgroundPolicy` bool structs with a free `ComputeBackgroundPolicy()` function. Works but introduces three types and a function for what is naturally a bitwise AND of flags. The flags enum is more extensible (add `Audio` with one enumerator) and eliminates the type-safety risk of mixing prefs/policy since both sides are conceptually the same domain.

### Pattern 3: OverlayStack as Non-Owning Execution View

**What:** A sorted, filtered view over Application-owned overlays.

**Data structure:** `std::vector<OverlayEntry>` sorted by effective priority, where:

```cpp
struct OverlayEntry
{
    IOverlay* Overlay;                // non-owning
    std::type_index Type;
    CapabilitySet RequiredCapabilities;
    int32_t Priority;
    bool CapabilitySatisfied = false;  // computed on state transition
    bool ManuallyActive = true;        // toggled via ActivateOverlay/DeactivateOverlay
};
```

An overlay is active (ticked) when `CapabilitySatisfied and ManuallyActive`.

**Execution order:**
- **Input (OnEvent):** Iterate in reverse priority order (highest priority first = top-down). If `OnEvent` returns `true`, stop propagation.
- **Update (OnUpdate):** Iterate in priority order (all active overlays, after the active state).
- **Render (OnRender):** Iterate in priority order (bottom-up, state renders first, then overlays in registration order). This naturally layers overlays on top of the state.

**Capability recomputation:** On state transition, OverlayStack receives the new `CapabilitySet` and recomputes `CapabilitySatisfied` for all entries. This triggers `OnAttach` for newly satisfied overlays and `OnDetach` for newly unsatisfied ones.

### Pattern 4: Registrar-Manifest Pattern for State and Overlay Registration

**What:** Extend the existing AppBuilder pattern to register states and overlays at build time, producing processed outputs stored in AppDescriptor.

The project already has this exact pattern for subsystems (`SubsystemRegistry` -> `SubsystemManifest`), plugins (`PluginEntry` -> topological sort -> build order), and lifecycle hooks (`LifecycleHookRegistrar` -> `LifecycleHookManifest`). Phase 4 adds:

1. **StateRegistrar** -> **StateManifest** (stored in AppDescriptor)
   - Entries: type_index, factory, capabilities, transition graph, initial state flag
   - Finalise validates: initial state exists, all transition targets registered, BFS reachability
   - Produces: `std::unordered_map<std::type_index, ...>` of state descriptors + validated transition graph

2. **OverlayRegistrar** -> **OverlayManifest** (stored in AppDescriptor)
   - Entries: type_index, factory, required capabilities, priority, default active flag
   - Finalise validates: no duplicate types
   - Produces: sorted list of overlay descriptors

3. **StateUIRegistrar** -> entries stored in StateManifest per state
   - `ForState<T>().SetUI<U>()` stores a `std::function<std::unique_ptr<IStateUI>()>` factory keyed by state type_index

**AppBuilder surface additions:**

```cpp
// In AppBuilder:
template<std::derived_from<IApplicationState> T>
void AddState(StateDescriptorV2 descriptor);

template<std::derived_from<IApplicationState> TFrom, std::derived_from<IApplicationState> TTo>
void AddTransition();

template<std::derived_from<IOverlay> T>
void RegisterOverlay(OverlayDescriptor descriptor = {});

// ForState<T>() returns a per-state builder proxy:
template<std::derived_from<IApplicationState> T>
auto ForState() -> StateBuilder<T>;

// StateBuilder supports:
template<std::derived_from<IStateUI> U>
void SetUI();
```

### Pattern 5: EngineContextServiceProvider Adapter

**What:** A thin struct that wraps `EngineContext&` to satisfy the `ServiceProvider` concept.

```cpp
struct EngineContextServiceProvider
{
    EngineContext& Context;

    template<typename T>
    auto Get() -> T& { return Context.GetAppSubsystem<T>(); }

    template<typename T>
    auto TryGet() -> T* { return Context.TryGetAppSubsystem<T>(); }
};

static_assert(ServiceProvider<EngineContextServiceProvider>);
```

This is the correct adapter per the architecture docs. Simulation calls `Initialise(services)` where `services` is either `EngineContextServiceProvider` (live) or `StandaloneServiceProvider` (headless test).

### Pattern 6: IOverlay::OnEvent Return Type Change

**What:** Phase 1 declared `IOverlay::OnEvent` as `void`. D-07 requires it to return `bool` for event consumption.

The current interface in `IOverlay.h`:
```cpp
virtual void OnEvent(EngineContext& /*context*/, EventQueue& /*events*/) {}
```

Must change to:
```cpp
virtual auto OnEvent(EngineContext& /*context*/, EventQueue& /*events*/) -> bool { return false; }
```

This is a breaking change to the interface but no concrete implementations exist yet (Phase 1 only defined the interface). The default `return false` means existing code that doesn't override continues to work.

**Note:** The architecture docs show `OnEvent(EngineContext& ctx, Event& event)` with single-event signature, but the actual codebase uses `EventQueue&`. This should remain as `EventQueue&` for consistency with IApplicationState and IStateUI, with the OverlayStack handling per-event iteration internally.

### Anti-Patterns to Avoid

- **Distributed transition logic:** Never let OverlayStack, Simulation, or IStateUI independently observe transitions and react. The ASM's ProcessPending owns the entire cascade sequence.
- **Mid-frame state changes:** All state operations are deferred. RequestTransition/Push/Pop immediately returns; the actual transition happens at the defined frame boundary.
- **Virtual dispatch in hot paths for policy checks:** BackgroundPreferences / SuspensionPolicy should be cached on push, not queried every frame. They're structural properties of the state, not dynamic.
- **Hand-rolling state graph validation:** Reuse the existing `TopologicalSort` and BFS pattern from Phase 1's `StateMachine::Finalise()`.
- **Mixing ownership models:** Application owns states and overlays via `unique_ptr`. ASM and OverlayStack are non-owning views. Never transfer ownership after construction.

## Reference Engine Analysis

### How Other Engines Handle This

| Engine | State Management | Overlay/Layer System | Key Insight |
|--------|-----------------|---------------------|-------------|
| **Oxylus** | Module-based plugin system, no explicit FSM | No layer stack; modules compose rendering | Per-scene flecs world ownership -- validates D-10 (Simulation per-state) |
| **RavEngine** | World = state (multiple worlds tick, one renders) | Component-based UI (RmlUi per entity) | Hot-swappable worlds; background worlds continue ticking -- validates BackgroundPreferences model |
| **Bevy** | States trait + ComputedStates + SubStates hierarchy | No overlay concept; UI is ECS-based | Four-phase transition schedule (exit leaf-to-root, enter root-to-leaf) -- validates deferred cascade model |
| **Spartan** | EngineMode flags (EditorVisible, Playing, Paused) | Widget system with visibility callbacks | Deferred entity ops prevent mid-frame invalidation -- validates deferred transition model |
| **Unreal** | GameInstance -> GameMode -> GameState hierarchy | Slate/UMG widget tree, separate from game state | Push/pop via GameMode stack; viewport manages UI layers, not a flat overlay system |

### Key Lessons from Reference Engines

1. **Bevy's four-phase transition model** (exit bottom-up, transition, enter top-down) is the gold standard for deterministic lifecycle ordering. Wayfinder's ProcessPending cascade should follow this: exit old state first, then enter new state, then update dependents (overlays, hooks, UI).

2. **RavEngine's "background worlds continue ticking"** validates the BackgroundPreferences/SuspensionPolicy model. Multiple simulation contexts can coexist; the question is which ones get ticked.

3. **Oxylus's per-scene flecs::world** confirms that world ownership per-state (D-10) is a proven pattern. Each Scene owns its world; Simulation owns its world similarly.

4. **Spartan's deferred entity operations** pattern (pending_add/pending_remove processed once per frame) directly validates the single-pending-operation model for state transitions.

5. **All engines avoid mid-frame state changes.** Every engine either defers transitions to frame boundaries or uses explicit processing points. This is universal best practice.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| State graph validation | Custom graph validator | Extend Phase 1 BFS reachability + existing `TopologicalSort` | Already proven and tested in StateMachine::Finalise() |
| Type-safe state keying | Custom type ID system | `std::type_index` | Standard, used throughout the codebase (SubsystemRegistry, AppBuilder, LifecycleHooks) |
| Capability checking | Custom bitfield system | Existing `CapabilitySet` (TagContainer::HasAll) | Phase 1/2 already provides the full capability infrastructure |
| ECS singleton management | Custom singleton pattern | `flecs::world::set<T>()` / `flecs::world::get<T>()` | flecs singletons are well-tested, used for ActiveGameState already |
| Deferred operation variant | Hand-rolled union or enum+data | `std::variant` with `std::visit` | Type-safe, exhaustive pattern matching, zero overhead |

## Common Pitfalls

### Pitfall 1: Transition Cascade Ordering Bugs
**What goes wrong:** OnAttach for a new overlay fires before the new state's capabilities are computed, so the overlay sees stale capability data and doesn't activate.
**Why it happens:** The cascade steps are not executed in the correct order.
**How to avoid:** Define the cascade as a numbered sequence in ProcessPending. Test each step's preconditions. The order must be: state exit -> state enter -> capability recompute -> overlay activation -> lifecycle hooks -> UI lifecycle.
**Warning signs:** Overlays that work on the second frame but not the first after a transition.

### Pitfall 2: Double-Transition in a Single Frame
**What goes wrong:** State A's OnUpdate requests transition to B, then B's OnEnter immediately requests transition to C, executing two transitions in one ProcessPending call.
**Why it happens:** ProcessPending enters the new state, and the new state's OnEnter queues another transition, which is then processed in the same pass.
**How to avoid:** ProcessPending executes exactly one operation. Any transition requested during OnEnter/OnExit is queued for the NEXT frame's ProcessPending. The pending operation slot is checked and set, not recursively processed.
**Warning signs:** States that skip frames or never receive OnUpdate.

### Pitfall 3: Push/Pop Stack Underflow
**What goes wrong:** RequestPop is called when only the root state is on the stack, causing undefined behaviour or assertion failure.
**Why it happens:** Game logic doesn't track stack depth.
**How to avoid:** RequestPop asserts (or returns error) when the stack has only one entry. The root state cannot be popped -- it can only be replaced via flat transition.
**Warning signs:** Crashes on escape key press at the top-level state.

### Pitfall 4: Stale Capability Set After Push
**What goes wrong:** Pushed state provides new capabilities, but overlays that depend on combined parent + child capabilities don't activate because only the top state's capabilities are used.
**Why it happens:** Effective capability set computation doesn't account for the full stack.
**How to avoid:** On push, the effective capability set = app capabilities + union of all active states' capabilities (states in the stack that are not fully suspended). Or more simply: app caps + active (top) state caps, since suspended states don't contribute capabilities. The architecture doc says "app capabilities + active state capabilities" -- only the top state's capabilities are used. This is correct because suspended states surrender their capabilities.
**Warning signs:** Rendering overlay disappearing when PauseState (which doesn't provide Rendering) is pushed over GameplayState.

### Pitfall 5: IStateUI Lifecycle Mismatch with State Lifecycle
**What goes wrong:** IStateUI::OnAttach is called but OnDetach isn't called during a flat transition, leaking resources.
**Why it happens:** The flat transition path (replace) doesn't mirror the push/pop path, and the IStateUI lifecycle forwarding only handles one case.
**How to avoid:** All three transition types (flat, push, pop) must go through a common lifecycle pathway that handles IStateUI. Extract helper methods: `SuspendState(type_index)`, `ResumeState(type_index)`, `EnterState(type_index)`, `ExitState(type_index)` that bundle the state + IStateUI lifecycle calls.
**Warning signs:** Memory growth across state transitions; UI elements from old states visible in new states.

### Pitfall 6: IOverlay::OnEvent Signature Change Breaking Existing Code
**What goes wrong:** Changing `void OnEvent(...)` to `auto OnEvent(...) -> bool` in IOverlay.h breaks any existing mock or test implementations.
**Why it happens:** Interface change during Phase 4 when Phase 1 defined the original.
**How to avoid:** Update all mock implementations in tests simultaneously. The change is safe because no concrete overlays exist yet -- only test mocks.
**Warning signs:** Compilation errors in `ApplicationStateTests.cpp` after the interface change.

## C++23 Patterns and Idioms

### Recommended C++23 Features for Phase 4

| Feature | Use Case | Benefit |
|---------|----------|---------|
| `std::variant` + `std::visit` | Pending operation type (`FlatTransition`/`PushTransition`/`PopTransition`) | Exhaustive compile-time handling of all operation types |
| `std::derived_from` concept | Template constraints on `RequestTransition<T>()`, `AddState<T>()` | Compile-time safety, clear error messages on misuse |
| `std::type_index` | State keying, overlay keying, IStateUI factory keying | Consistent with SubsystemRegistry, AppBuilder patterns |
| `[[nodiscard]]` | All Result-returning methods, Finalise(), GetActiveState() | Prevents ignoring errors or dangling access |
| `constexpr` functions | `ComputeBackgroundPolicy()` | Evaluated at compile time for static configurations |
| Designated initialisers | `StateRegistrationDescriptor{.Initial = true, .Capabilities = {...}}` | Readable, self-documenting registration calls |
| `std::span` | Read-only views of modal stack, overlay entries | Non-owning, bounds-safe iteration |
| `auto(x)` decay-copy | Capturing type_index values in lambdas | Prevents dangling references to temporaries |

### std::variant for Deferred Operations

The pending operation model benefits from `std::variant` over a manual enum+union:

```cpp
using PendingOperation = std::variant<
    std::monostate,      // no operation pending
    FlatTransition,      // replace active state
    PushTransition,      // push modal state
    PopTransition        // pop modal state
>;

void ApplicationStateMachine::ProcessPending(EngineContext& context)
{
    auto operation = std::exchange(m_pending, std::monostate{});

    std::visit([this, &context](auto&& op)
    {
        using T = std::decay_t<decltype(op)>;
        if constexpr (std::same_as<T, std::monostate>)
        {
            return; // nothing to do
        }
        else if constexpr (std::same_as<T, FlatTransition>)
        {
            ExecuteFlatTransition(context, op.Target);
        }
        else if constexpr (std::same_as<T, PushTransition>)
        {
            ExecutePush(context, op.Target);
        }
        else if constexpr (std::same_as<T, PopTransition>)
        {
            ExecutePop(context);
        }
        else
        {
            static_assert(ALWAYS_FALSE<T>, "Unhandled pending operation type");
        }
    }, operation);
}
```

This is exhaustive at compile time -- adding a new operation type without handling it is a compile error.

### std::exchange for Clean State Swaps

```cpp
// Atomically take the pending operation and reset it
auto operation = std::exchange(m_pending, std::monostate{});

// Safely swap active state
auto* previousState = std::exchange(m_activeState, newState);
```

### Concepts for Builder API Safety

```cpp
// Already used in AppBuilder for plugins, extend to states and overlays:
template<std::derived_from<IApplicationState> T>
void AddState(StateRegistrationDescriptor descriptor = {});

template<std::derived_from<IOverlay> T>
void RegisterOverlay(OverlayDescriptor descriptor = {});

template<std::derived_from<IStateUI> U>
void StateBuilder::SetUI();
```

These give clear diagnostic messages when a user tries to register a type that doesn't derive from the correct base.

## Code Examples

### ApplicationStateMachine Registration and Validation

```cpp
// In AppBuilder::Finalise() or a dedicated StateRegistrar::Finalise():
auto ApplicationStateMachine::Finalise(std::type_index initialState) -> Result<void>
{
    if (not m_states.contains(initialState))
    {
        return MakeError("Initial state not registered");
    }

    // Validate all transition targets exist
    for (const auto& [from, targets] : m_transitions)
    {
        for (const auto& to : targets)
        {
            if (not m_states.contains(to))
            {
                return MakeError(std::format("Transition target not registered"));
            }
        }
    }

    // BFS reachability from initial state (same pattern as Phase 1 StateMachine)
    std::unordered_set<std::type_index> visited;
    std::queue<std::type_index> frontier;
    frontier.push(initialState);
    visited.insert(initialState);

    while (not frontier.empty())
    {
        auto current = frontier.front();
        frontier.pop();

        if (auto it = m_transitions.find(current); it != m_transitions.end())
        {
            for (const auto& target : it->second)
            {
                if (not visited.contains(target))
                {
                    visited.insert(target);
                    frontier.push(target);
                }
            }
        }

        // Also consider push targets reachable (any state can be pushed to from flat-reachable states)
    }

    if (visited.size() != m_states.size())
    {
        return MakeError("Unreachable states detected in state graph");
    }

    m_initialState = initialState;
    m_finalised = true;
    return {};
}
```

### Flat Transition Execution

```cpp
void ApplicationStateMachine::ExecuteFlatTransition(EngineContext& context, std::type_index target)
{
    auto activeType = m_stack.back();

    // 1. Exit old state (+ IStateUI)
    ExitState(context, activeType);

    // 2. Replace top of stack
    m_stack.back() = target;

    // 3. Enter new state (+ IStateUI)
    EnterState(context, target);

    // 4. Recompute capabilities and cascade to overlays
    RecomputeCapabilitiesAndCascade(context);
}
```

### Push/Pop Execution with Negotiation

```cpp
void ApplicationStateMachine::ExecutePush(EngineContext& context, std::type_index target)
{
    auto activeType = m_stack.back();
    auto* activeState = m_states.at(activeType).get();

    // 1. Compute negotiated background policy
    auto* pushedState = m_states.at(target).get();
    auto policy = ComputeBackgroundPolicy(
        activeState->GetBackgroundPreferences(),
        pushedState->GetSuspensionPolicy());
    m_backgroundPolicies[activeType] = policy;

    // 2. Suspend current state (+ IStateUI)
    SuspendState(context, activeType);

    // 3. Push new state onto stack
    m_stack.push_back(target);

    // 4. Enter pushed state (+ IStateUI)
    EnterState(context, target);

    // 5. Recompute capabilities (only top state contributes)
    RecomputeCapabilitiesAndCascade(context);
}

void ApplicationStateMachine::ExecutePop(EngineContext& context)
{
    WAYFINDER_ASSERT(m_stack.size() > 1, "Cannot pop the root state");

    auto activeType = m_stack.back();

    // 1. Exit popped state (+ IStateUI)
    ExitState(context, activeType);

    // 2. Pop from stack
    m_stack.pop_back();
    m_backgroundPolicies.erase(activeType);

    // 3. Resume now-active state (+ IStateUI)
    auto resumedType = m_stack.back();
    ResumeState(context, resumedType);

    // 4. Recompute capabilities
    RecomputeCapabilitiesAndCascade(context);
}
```

### OverlayStack Capability Update

```cpp
void OverlayStack::UpdateCapabilities(const CapabilitySet& effectiveCaps, EngineContext& context)
{
    for (auto& entry : m_entries)
    {
        const bool wasSatisfied = entry.CapabilitySatisfied;
        entry.CapabilitySatisfied = entry.RequiredCapabilities.IsEmpty()
            or effectiveCaps.HasAll(entry.RequiredCapabilities);

        const bool wasActive = wasSatisfied and entry.ManuallyActive;
        const bool isActive = entry.CapabilitySatisfied and entry.ManuallyActive;

        if (not wasActive and isActive)
        {
            auto result = entry.Overlay->OnAttach(context);
            if (not result)
            {
                Log::Warn(LogEngine, "Overlay '{}' OnAttach failed: {}",
                    entry.Overlay->GetName(), result.error().GetMessage());
                entry.ManuallyActive = false;
            }
        }
        else if (wasActive and not isActive)
        {
            auto result = entry.Overlay->OnDetach(context);
            if (not result)
            {
                Log::Warn(LogEngine, "Overlay '{}' OnDetach failed: {}",
                    entry.Overlay->GetName(), result.error().GetMessage());
            }
        }
    }
}
```

### Simulation Class (Thin World Wrapper)

```cpp
class Simulation : public StateSubsystem
{
public:
    [[nodiscard]] auto Initialise(EngineContext& context) -> Result<void> override;
    void Shutdown() override;

    void Update(float deltaTime);

    [[nodiscard]] auto GetWorld() -> flecs::world&;
    [[nodiscard]] auto GetWorld() const -> const flecs::world&;
    [[nodiscard]] auto GetCurrentScene() -> Scene*;
    [[nodiscard]] auto GetCurrentScene() const -> const Scene*;

    void LoadScene(std::string_view scenePath);
    void UnloadCurrentScene();

private:
    flecs::world m_world;
    std::unique_ptr<Scene> m_currentScene;
};
```

Note: Simulation IS a StateSubsystem (D-11). Its `Initialise(EngineContext&)` matches the subsystem interface. It accesses services via `context.GetAppSubsystem<T>()` directly, not through ServiceProvider -- the ServiceProvider concept is only needed for the `StandaloneServiceProvider` headless test path. The actual Simulation class can provide a separate `Initialise(ServiceProvider auto& services)` overload or use EngineContext directly.

**Refinement:** Since Simulation is a StateSubsystem, it participates in SubsystemManifest lifecycle automatically. The `Initialise(EngineContext&)` override IS the ServiceProvider path for the live case. For headless tests, the test constructs Simulation directly and calls a separate init method with StandaloneServiceProvider. This avoids over-engineering the live path.

### EngineContext Wiring (Phase 4 Implementation of Stubs)

```cpp
// In EngineContext.h -- replace assert stubs with delegation to ASM/OverlayStack:

template<std::derived_from<IApplicationState> T>
void RequestTransition()
{
    WAYFINDER_ASSERT(m_stateMachine, "ApplicationStateMachine not set");
    m_stateMachine->RequestTransition<T>();
}

template<std::derived_from<IApplicationState> T>
void RequestPush()
{
    WAYFINDER_ASSERT(m_stateMachine, "ApplicationStateMachine not set");
    m_stateMachine->RequestPush<T>();
}

void RequestPop()
{
    WAYFINDER_ASSERT(m_stateMachine, "ApplicationStateMachine not set");
    m_stateMachine->RequestPop();
}

void ActivateOverlay(std::type_index overlayType)
{
    WAYFINDER_ASSERT(m_overlayStack, "OverlayStack not set");
    m_overlayStack->Activate(overlayType);
}

void DeactivateOverlay(std::type_index overlayType)
{
    WAYFINDER_ASSERT(m_overlayStack, "OverlayStack not set");
    m_overlayStack->Deactivate(overlayType);
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `Game` owns world + subsystems + state machine | Simulation owns world + scene only; subsystems in SubsystemRegistry; state machine as member | This migration (Phase 4) | Clean separation of concerns |
| `LayerStack` with push/pop layers | OverlayStack with capability-gated persistent overlays | This migration (Phase 4) | Overlays auto-activate/deactivate based on state capabilities |
| `Plugin::OnStartup/OnShutdown` lifecycle | `IPlugin::Build()` + lifecycle hooks in AppDescriptor | Phase 3 (completed) | Declarative, validated at Finalise() |
| `GameStateMachine` as GameSubsystem | `ApplicationStateMachine` with type_index keying + `StateMachine<TStateId>` for sub-states | This migration (Phase 4) | Type-safe, validated graph, push/pop modal support |
| `EventQueue` dispatch to layers | OverlayStack top-down consumption + state event handling | This migration (Phase 4) | Explicit consumption model, deterministic ordering |

## Open Questions

1. **Capability set during push/pop stack:**
   - What we know: The effective capability set = app caps + active state caps (per architecture doc).
   - What's unclear: Does "active state" mean only the top of the stack, or the union of all non-fully-suspended states? The architecture doc says "active state capabilities" suggesting only the top.
   - Recommendation: Only the top state contributes state-level capabilities. This is simpler and matches the semantic that suspended states surrender control. If GameplayState provides Simulation+Rendering and PauseState only provides Presentation, then during pause the Rendering-gated RenderGraphOverlay deactivates. This seems correct -- the pause state doesn't need the render graph overlay. If this proves wrong, it's a one-line change to union all stack states' capabilities.

2. **ActiveGameState update timing:**
   - What we know: Updated via transition callbacks on the sub-state machine (D-12, D-07 from game_framework.md).
   - What's unclear: ActiveGameState is an ECS singleton. It's updated by GameplayState's sub-state machine OnTransition callback, not by the ASM. Phase 4 only needs to ensure the ECS world exists (inside Simulation) when the callback fires.
   - Recommendation: ActiveGameState update is entirely GameplayState-internal. Phase 4 provides the mechanism (ASM lifecycle) but not the policy (which ECS singleton to update). The SIM-07 requirement is satisfied by the OnTransition callback pattern demonstrated in game_framework.md.

3. **OverlayStack OnAttach/OnDetach timing:**
   - What we know: Overlays are persistent (created at startup). OnAttach/OnDetach fire when capability satisfaction changes.
   - What's unclear: Should OnAttach fire during initial startup for overlays with empty RequiredCapabilities?
   - Recommendation: Yes. All overlays with `DefaultActive = true` and satisfied capabilities should receive OnAttach during the initial state enter sequence. This makes the lifecycle consistent -- OnAttach always fires before the first OnUpdate.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | doctest (via `doctest::doctest_with_main`) |
| Config file | None (header-only, linked via CMake) |
| Quick run command | `cmake --build --preset debug --target wayfinder_core_tests && ctest --preset test -R core` |
| Full suite command | `ctest --preset test` |

### Phase Requirements -> Test Map
| Req ID | Behaviour | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| STATE-02 | ASM flat transitions and push/pop | unit | `wayfinder_core_tests -tc="ApplicationStateMachine*"` | No - Wave 0 |
| STATE-03 | Deferred transitions at frame boundaries | unit | `wayfinder_core_tests -tc="ApplicationStateMachine*deferred*"` | No - Wave 0 |
| STATE-04 | Declared transitions only, validation at Finalise | unit | `wayfinder_core_tests -tc="ApplicationStateMachine*validation*"` | No - Wave 0 |
| STATE-05 | Push/pop negotiation (BackgroundPreferences AND SuspensionPolicy) | unit | `wayfinder_core_tests -tc="ApplicationStateMachine*negotiation*"` | No - Wave 0 |
| STATE-08 | State subsystems persist across push/pop | unit | `wayfinder_core_tests -tc="ApplicationStateMachine*push*subsystem*"` | No - Wave 0 |
| SIM-01 | Simulation replaces Game (world + scene) | unit | `wayfinder_core_tests -tc="Simulation*"` | No - Wave 0 |
| SIM-03 | EngineContextServiceProvider adapter | unit | `wayfinder_core_tests -tc="ServiceProvider*EngineContext*"` | No - Wave 0 |
| SIM-06 | Sub-states are state-internal | unit | Verified by existing `StateMachineTests.cpp` | Yes |
| SIM-07 | ActiveGameState updated via callbacks | unit | `wayfinder_core_tests -tc="Simulation*ActiveGameState*"` | No - Wave 0 |
| OVER-02 | OverlayStack execution order (input/render) | unit | `wayfinder_core_tests -tc="OverlayStack*"` | No - Wave 0 |
| OVER-03 | Capability-gated overlay activation | unit | `wayfinder_core_tests -tc="OverlayStack*capability*"` | No - Wave 0 |
| OVER-04 | Runtime toggle | unit | `wayfinder_core_tests -tc="OverlayStack*toggle*"` | No - Wave 0 |
| UI-02 | IStateUI registration via builder | unit | `wayfinder_core_tests -tc="*StateUI*registration*"` | No - Wave 0 |
| UI-03 | IStateUI lifecycle mirrors state | unit | `wayfinder_core_tests -tc="*StateUI*lifecycle*"` | No - Wave 0 |

### Sampling Rate
- **Per task commit:** `cmake --build --preset debug --target wayfinder_core_tests && ctest --preset test -R core`
- **Per wave merge:** `ctest --preset test`
- **Phase gate:** Full suite green before `/gsd-verify-work`

### Wave 0 Gaps
- [ ] `tests/app/ApplicationStateMachineTests.cpp` -- covers STATE-02, STATE-03, STATE-04, STATE-05, STATE-08
- [ ] `tests/app/OverlayStackTests.cpp` -- covers OVER-02, OVER-03, OVER-04
- [ ] `tests/gameplay/SimulationTests.cpp` -- covers SIM-01, SIM-03, SIM-07
- [ ] `tests/app/StateUITests.cpp` or `tests/plugins/StateUITests.cpp` -- covers UI-02, UI-03
- [ ] Update `tests/CMakeLists.txt` to add new test files to `wayfinder_core_tests`
- [ ] Update mock classes in `ApplicationStateTests.cpp` for IOverlay::OnEvent return type change

## Sources

### Primary (HIGH confidence)
- Existing codebase: `StateMachine.h`, `SubsystemRegistry.h`, `SubsystemManifest.h`, `AppBuilder.h`, `EngineContext.h`, `LifecycleHooks.h`, `ServiceProvider.h` -- verified patterns and idioms
- Architecture docs: `application_architecture_v2.md`, `application_migration_v2.md`, `game_framework.md` -- canonical design specifications
- Phase 1/2/3 implementations -- proven registrar-Finalise()-manifest pattern

### Secondary (MEDIUM confidence)
- Reference engines (Oxylus, RavEngine, Bevy, Spartan) -- architectural validation, pattern comparison
- C++23 standard features -- `std::variant`, `std::visit`, `std::exchange`, concepts, `std::span`

### Tertiary (LOW confidence)
- None -- all findings verified against codebase or official documentation

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- entirely C++ standard library + existing engine primitives, no external dependencies
- Architecture: HIGH -- builds on three completed phases of proven patterns (registrar-manifest, type_index keying, capability gating)
- Pitfalls: HIGH -- derived from real patterns observed in the codebase and reference engines
- C++23 idioms: HIGH -- features already used elsewhere in the codebase (std::expected via Result, concepts, designated initialisers)

**Research date:** 2026-04-06
**Valid until:** Indefinite (internal architecture, no external dependencies to go stale)
