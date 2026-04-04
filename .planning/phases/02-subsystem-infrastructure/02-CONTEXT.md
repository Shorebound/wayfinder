# Phase 2: Subsystem Infrastructure - Context

**Gathered:** 2026-04-04
**Status:** Ready for planning

<domain>
## Phase Boundary

SubsystemRegistry and EngineContext v2 form the proven spine of the new architecture with dependency ordering and capability activation. This phase delivers: SubsystemRegistry with topological DependsOn ordering, cycle detection at Finalise(), capability-gated activation, abstract-type resolution, Result<void> error propagation, EngineContext v2 as non-owning facade with typed subsystem access, and removal of the static GameSubsystems accessor.

</domain>

<decisions>
## Implementation Decisions

### Registry API Shape
- **D-01:** Finalise-then-Initialise split as an internal-only detail. Developers register subsystems in their plugin's `Build()` method. `AppBuilder::Finalise()` calls `SubsystemRegistry::Finalise()` internally (validates dependency graph, detects cycles, freezes registration). Then `SubsystemRegistry::Initialise()` creates instances in topological order. The game developer never calls either method directly.
- **D-02:** Descriptor-based registration using designated initialisers. All metadata (RequiredCapabilities, DependsOn) in a single aggregate block. Consistent with StateMachine descriptor pattern from Phase 1. Fluent builder API noted as a potential future DX improvement if registration APIs get a cohesion pass.
- **D-03:** Two-template-parameter abstract type resolution: `Register<SDL3Window, Window>({...})`. Queryable by either concrete type (`SDL3Window`) or abstract type (`Window`). Duplicate abstract registrations caught at `Finalise()` as a validation error.
- **D-04:** Scope enforced via template parameter: `SubsystemRegistry<AppSubsystem>` and `SubsystemRegistry<StateSubsystem>`. Compile-time scope enforcement.
- **D-05:** Remove static `GameSubsystems` accessor entirely (no rename to `StateSubsystems`). Replace with ECS singleton component pattern: store an `EngineContextRef` singleton in the flecs world during state entry, remove on state exit. Flecs systems access engine subsystems via `world.get<EngineContextRef>()`. Thread-safe by design (flecs controls singleton access through staging model), eliminates global mutable state.
- **D-06:** Subsystem `Initialise(EngineContext&) -> Result<void>`. Receives full EngineContext, not just its own-scope registry, because state subsystems commonly need app subsystems (e.g., PhysicsSubsystem needs TimeSubsystem from app scope). Scoping safety comes from the dependency graph validation, not from restricting the parameter type.
- **D-07:** Shutdown in reverse topological order (mirrors init order). If A depends on B, B shuts down after A. Automatic from the dependency graph.
- **D-08:** Two-tier access pattern from Phase 1 ServiceProvider: `Get<T>()` asserts and returns `T&` (declared dependency must exist post-Finalise). `TryGet<T>()` returns `T*` (optional access, returns nullptr if capability-gated off).

### EngineContext v2 Design
- **D-09:** Full interface declared in Phase 2 with assert stubs for Phase 4 features. Method signatures for state transitions (`RequestTransition<T>()`, `RequestPush<T>()`, `RequestPop()`), overlay operations (`ActivateOverlay`, `DeactivateOverlay`), and `GetAppDescriptor()` exist but assert if called before Phase 4 implementation. Locks the contract early.
- **D-10:** Application owns subsystem registries, state machine, overlay stack as value members (concrete types, not polymorphic, no heap allocation needed). EngineContext is a non-owning facade holding raw pointers to Application's members. Lifetime is architectural: Application outlives EngineContext. Name matches semantics - "context" is a view, not an owner.
- **D-11:** Old 27-line EngineContext struct deleted and replaced in-place with the new class. Same file, same name, completely different implementation. No legacy coexistence needed - no external consumers, greenfield development.
- **D-12:** Single EngineContext type with mixed const/non-const. Subsystem queries are const (registries don't mutate after init). State transitions and overlay ops are non-const (they queue deferred commands). No split into read/write types - deferred command pattern provides runtime safety, const correctness provides compile-time guidance. Split into read/write handles later if threading demands it.
- **D-13:** Directly constructible with partial pointers for headless tests. No Application needed. Null pointers for unneeded features (state machine, overlays). Enables unit testing of subsystem init without full application bootstrap.
- **D-14:** Single EngineContext instance passed by reference to all lifecycle methods (OnEnter, OnUpdate, etc.) and subsystem `Initialise()`. Same instance throughout the application. All access flows from Application -> states -> ECS world singleton.

### Capability Activation Flow
- **D-15:** Union model for effective capability set. App-level capabilities reflect hardware/config (set at startup: Rendering if GPU present, Presentation if window available). State-level capabilities reflect domain needs (declared per-state: GameplayState provides Simulation). Effective set = union of both. Headless tests naturally get only `{Simulation}` because no app caps are set.
- **D-16:** Batched capability changes on state transition. Compute new effective set, diff with current, atomically swap, then activate/deactivate subsystems. No intermediate state where capabilities are partially applied. Shared capabilities across states stay active (no teardown+reinit for subsystems common to both old and new state).
- **D-17:** HasAll semantics for RequiredCapabilities check. Every required capability must be present in the effective set for a subsystem to activate. HasAny mode deferred until a real use case emerges (non-breaking addition to descriptor).

### Error Handling and Validation
- **D-18:** Cycle detection produces full cycle path in error message: "Cycle detected: Renderer -> Window -> Renderer". Returned as `Result<void>` error from `Finalise()`.
- **D-19:** Fail-fast on `Initialise()` failure. First subsystem failure aborts the init sequence. Already-initialised subsystems shut down in reverse topological order (RAII). Error surfaces to `Application::Run()` which logs and returns.
- **D-20:** No optional subsystem flag. Capabilities are the optionality mechanism. If a subsystem's required capabilities aren't met, it's never created (no failure possible). If capabilities are met and Initialise fails, it's a hard error. Add Optional descriptor field later if a real use case (telemetry, mod loading) emerges.
- **D-21:** Finalise() catches structural errors as primary validation (unregistered dependencies, incompatible capability requirements, duplicate abstract registrations, cycles). `Get<T>()` assert is a safety net, not the primary error path.

### Agent's Discretion
- Exact topological sort algorithm (Kahn's vs DFS-based) - agent chooses based on what integrates cleanly with the descriptor storage
- Internal storage data structures for SubsystemRegistry (type_index maps, adjacency lists, etc.)
- Whether `EngineContextRef` for the flecs singleton is a simple struct with a pointer or something slightly richer
- Error message formatting details beyond "full cycle path" and "which subsystem failed"
- Whether `Deps<A, B>()` type list helper is worth adding to sweeten the DependsOn descriptor field

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Architecture Specifications
- `docs/plans/application_architecture_v2.md` -- SubsystemRegistry design (dependency ordering, capability gating, abstract-type resolution), EngineContext v2 class design (ownership, API surface, lifecycle), capability system (two-source model, activation rules)
- `docs/plans/application_migration_v2.md` -- Transition tables for SubsystemCollection -> SubsystemRegistry, EngineContext struct -> class, GameSubsystems -> StateSubsystems (now: removal + ECS singleton)
- `docs/plans/game_framework.md` -- ServiceProvider concept, Simulation access patterns, how states interact with EngineContext

### Existing Types (to build on or evolve)
- `engine/wayfinder/src/app/Subsystem.h` -- Current SubsystemCollection<TBase> template. Evolves into SubsystemRegistry with dependency ordering. Current Subsystem/AppSubsystem/StateSubsystem base classes.
- `engine/wayfinder/src/app/EngineContext.h` -- Current 27-line struct. Replaced in-place with new EngineContext class.
- `engine/wayfinder/src/app/EngineRuntime.h` + `.cpp` -- Current service creation/init/shutdown pattern. Shows how subsystems are currently managed (manual sequential creation).
- `engine/wayfinder/src/gameplay/Capability.h` -- CapabilitySet alias and well-known capability tags (Simulation, Rendering, Presentation, Editing). Phase 2 uses these for gated activation.
- `engine/wayfinder/src/gameplay/Tag.h` -- Tag + TagContainer. CapabilitySet is TagContainer. HasAll/HasAny/AddTags methods used for capability checks and set computation.
- `engine/wayfinder/src/core/Result.h` -- Result<T, TError> alias for std::expected. Used for Initialise() and Finalise() return types.
- `engine/wayfinder/src/app/IApplicationState.h` -- V2 state interface. OnEnter/OnExit take EngineContext& parameter.
- `engine/wayfinder/src/app/AppSubsystem.h` -- V2 app-scoped subsystem base class.
- `engine/wayfinder/src/app/StateSubsystem.h` -- V2 state-scoped subsystem base class.

### Supporting Types
- `engine/wayfinder/src/core/InternedString.h` -- O(1) equality, std::hash. Used in Tag system which backs CapabilitySet.
- `engine/wayfinder/src/core/events/EventQueue.h` -- EventQueue referenced by EngineContext v2 (states receive it for event processing).
- `engine/wayfinder/src/gameplay/GameStateMachine.h` -- Current state machine. Shows existing Bind/Unbind pattern for static accessor that Phase 2 removes.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `SubsystemCollection<TBase>`: Template container with Register<T>/Get<T>/Initialise/Shutdown. Core API shape is preserved; enhanced with dependency ordering, capability gating, and abstract-type resolution.
- `TagContainer::HasAll()` / `HasAny()` / `AddTags()`: Capability check and set computation operations already implemented.
- `Result<T, TError>`: Error propagation pattern from core/Result.h. Used for Initialise() and Finalise() return types.
- `std::type_index`: Already used as key type in SubsystemCollection storage. Extends naturally to abstract-type dual registration.

### Established Patterns
- Descriptor-based registration with Finalise validation (StateMachine<TStateId> from Phase 1)
- SubsystemCollection Register<T>/Get<T> ergonomic API (preserved in SubsystemRegistry)
- GameSubsystems static Bind/Unbind accessor (replaced by ECS singleton component)
- EngineRuntime manual sequential init with Result<void> (pattern preserved, ordering now automated)

### Integration Points
- `EngineContext.h`: Replaced in-place. All current consumers (EngineRuntime, Game, states) must update to new API.
- `SubsystemCollection` usages: EngineRuntime uses SubsystemCollection<AppSubsystem>, Game uses SubsystemCollection<GameSubsystem>. Both migrate to SubsystemRegistry<T>.
- `GameSubsystems::Get<T>()` callsites (flecs systems, subsystems): All migrate to `EngineContextRef` ECS singleton pattern.
- `CMakeLists.txt`: New/renamed/removed files must be updated in source file lists.
- `IApplicationState::OnEnter(EngineContext&)`: Already takes EngineContext by reference. No signature change needed when EngineContext is replaced in-place.

</code_context>

<specifics>
## Specific Ideas

- Topological sort reusable from rendering graph's pass dependency ordering (Kahn's algorithm) -- check if it can be extracted into a shared utility
- `Deps<Window, TimeSubsystem>()` variadic template helper could sweeten the DependsOn descriptor field by avoiding repeated `typeid()` calls
- EngineContext assert stubs for Phase 4 should use `WF_ASSERT(false, "...")` pattern, not exceptions
- ECS singleton pattern (`EngineContextRef`) should be a simple struct with a raw pointer: `struct EngineContextRef { EngineContext* Context; };` -- no RAII, no ownership, just a flecs-compatible component

</specifics>

<deferred>
## Deferred Ideas

None -- discussion stayed within phase scope.

</deferred>

---

*Phase: 02-subsystem-infrastructure*
*Context gathered: 2026-04-04*
