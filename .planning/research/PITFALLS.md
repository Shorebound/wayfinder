# Domain Pitfalls

**Domain:** C++23 game engine architecture migration (monolithic to plugin-composed)
**Project:** Wayfinder
**Researched:** 2026-04-03
**Overall confidence:** HIGH (grounded in actual codebase analysis, not generic advice)

---

## Critical Pitfalls

Mistakes that cause rewrites, subtle UB, or broken test suites.

---

### Pitfall 1: State Machine Re-Entrancy During Transitions

**What goes wrong:** `ApplicationStateMachine` processes a transition (calling `OnExit` on the old state, `OnEnter` on the new state), and code inside those callbacks requests *another* transition via `EngineContext::RequestTransition<T>()`. If the state machine processes requests eagerly, the second transition fires mid-lifecycle of the first, corrupting the state stack and possibly double-destroying subsystems.

**Why it happens in Wayfinder:** The current `GameStateMachine::TransitionTo()` in [Game.cpp](engine/wayfinder/src/gameplay/Game.cpp) is synchronous and immediate. The v2 plan says transitions are "deferred: queued during the frame, processed at the start of the next frame" -- but unless this is enforced at the `EngineContext` level, an `OnEnter`/`OnExit` callback that calls `ctx.RequestTransition<T>()` directly could still trigger immediate processing if the queue drain is re-entrant.

**Consequences:**
- State-scoped subsystems destroyed twice (double-free / UB)
- `OnExit` called on a state that never finished `OnEnter` (invariant violations)
- Push/pop stack corruption: a pop during a push leaves the stack in an impossible state

**Warning signs:**
- Any `ctx.RequestTransition` or `ctx.RequestPush` call inside `OnEnter`, `OnExit`, `OnSuspend`, or `OnResume`
- Tests that work with simple A->B transitions but crash on A->B->C chains

**Prevention:**
1. `ApplicationStateMachine` must reject or queue (never execute) transitions while a transition is already in progress. Add a `m_transitioning` guard flag.
2. Assert at the `RequestTransition` call site if called during an active transition callback: `WAYFINDER_ASSERT(not m_processingTransition, "Transition requested during active transition")`
3. All transition requests must go through the deferred queue -- no backdoor for "immediate" transitions.
4. Test: request a transition inside `OnEnter`, verify it is deferred to the next frame boundary and the first transition completes cleanly.

**Phase mapping:** Must be addressed in the ApplicationStateMachine implementation phase. Not fixable retroactively without redesign.

**CONCERNS.md connection:** The current `GameStateMachine` is synchronous (`TransitionTo` applies immediately). This pattern must not leak into v2.

---

### Pitfall 2: Subsystem Init-Order Dependencies Without Explicit Declaration

**What goes wrong:** `AppSubsystem` and `StateSubsystem` instances depend on each other at init time (e.g. `Renderer` needs `Window`; `PhysicsSubsystem` needs `AssetService`). Without explicit `DependsOn` declarations and topological sorting, the init order is fragile -- it works today because of registration order, then breaks when a plugin registers in a different order.

**Why it happens in Wayfinder:** The current `SubsystemCollection::Initialise()` in [Subsystem.h](engine/wayfinder/src/app/Subsystem.h) processes subsystems in **registration order**. There is no dependency declaration or topological sort. The v2 migration plan says "Add dependency ordering (`DependsOn`), topological sort" -- but this is listed as a modification of the existing `SubsystemCollection`, not a separate step. If the topological sort is deferred to "later" while subsystems are being decomposed from `EngineRuntime`, the implicit order breaks.

**Consequences:**
- `Subsystem::Initialise()` accesses a service that hasn't been created yet (null deref or assert)
- Shutdown in reverse init order destroys a dependency before its dependant, triggering use-after-free in `Shutdown()` callbacks
- Non-deterministic failures that only manifest when plugin registration order changes

**Warning signs:**
- Init order works in Journey but fails in a test that registers plugins differently
- Adding a new `AppSubsystem` breaks an unrelated subsystem's init

**Prevention:**
1. Implement `DependsOn` and topological sort for `SubsystemRegistry` **before** decomposing `EngineRuntime` into individual `AppSubsystem` instances. The sort is the prerequisite, not the follow-up.
2. Validate the dependency graph at build time (in `AppBuilder::Finalise()`): detect cycles, missing dependencies, and unreachable subsystems. Fail fast with a clear error listing the cycle.
3. Shutdown must be reverse topological order, not just reverse init order (they differ when dependencies form a DAG, not a chain).
4. Test: register subsystems in reverse order of their dependencies, verify init still succeeds.

**Phase mapping:** Subsystem scoping phase. Must be complete before EngineRuntime decomposition begins.

**CONCERNS.md connection:** "Introduce `AppSubsystem` / `StateSubsystem` scoping before decomposing `EngineRuntime`" is listed as priority #1.

---

### Pitfall 3: Static Accessor Migration Breaks Flecs Callbacks

**What goes wrong:** Three static accessors (`GameSubsystems::Get<T>()`, `BlendableEffectRegistry::GetActiveInstance()`, `SceneComponentRegistry::Get()`) are migrated to dependency injection via `EngineContext`. But flecs ECS system callbacks have a fixed signature -- they receive `(flecs::entity, Component&...)` and cannot accept additional context parameters. Code inside these callbacks that currently calls `GameSubsystems::Get<T>()` has no way to receive an injected `EngineContext`.

**Why it happens in Wayfinder:** The migration plan renames `GameSubsystems` to `StateSubsystems` -- same Bind/Unbind pattern. The static accessor exists specifically because flecs constraints prevent parameter injection into system callbacks. Removing the static entirely (replacing with DI) would require wrapping every flecs system callback in a lambda that captures a context pointer, or using flecs' `ctx()` system-level user data. Both approaches have trade-offs (lambda capture lifetime, type erasure overhead).

**Specific call sites at risk:**
- `ComponentRegistry.cpp` lines 669, 945, 1185: `BlendableEffectRegistry::GetActiveInstance()` during component deserialisation
- `Game.cpp` line 266: `GameSubsystems::Get<GameplayTagRegistry>()` during init
- Physics test fixtures: `GameSubsystems::Bind(&Subsystems)` / `Unbind()` in test setup/teardown

**Consequences:**
- If the static is removed prematurely, flecs callbacks silently get null and crash
- If the static is kept but renamed without updating all Bind/Unbind sites, the collection is never bound
- Test fixtures that call Bind/Unbind directly must be updated in lockstep or tests assert on startup

**Warning signs:**
- Assertion "GameSubsystems::Get() called before Game initialisation" firing in ECS callbacks
- Tests pass individually but fail in batch (static state leaking between tests)

**Prevention:**
1. Keep the static accessor pattern (renamed to `StateSubsystems`) for the entire v2 migration. Do not attempt to remove it in this milestone.
2. For `BlendableEffectRegistry`, the plan already notes this: "Remove static instance once ComponentRegistry supports context parameter." This is explicitly a post-migration cleanup.
3. Audit every Bind/Unbind call site during the rename. The rename from `GameSubsystems` to `StateSubsystems` must be atomic -- find-and-replace across engine + tests in a single commit.
4. Test fixtures (PhysicsTests.cpp, PhysicsIntegrationTests.cpp) already use Bind/Unbind correctly; keep the same pattern with the new name.

**Phase mapping:** Rename phase (early). The static accessor survives this milestone; only the name changes.

**CONCERNS.md connection:** "Static global accessors" section. All three are documented with their constraints.

---

### Pitfall 4: EngineRuntime Decomposition Creates Init-Time Circular Dependencies

**What goes wrong:** `EngineRuntime` currently constructs services in a hardcoded order: Input -> Time -> Window -> RenderDevice -> Renderer -> SceneRenderExtractor. When decomposed into individual `AppSubsystem` instances, some of these have bidirectional dependencies: `Renderer` needs `RenderDevice`, but `SceneRenderExtractor` needs both `Renderer` and the flecs world (which lives in `Simulation`, a state-scoped concept). If `SceneRenderExtractor` is made an `AppSubsystem` that depends on `Renderer`, but the flecs world isn't available until a state is entered, it can't initialise at app scope.

**Why it happens in Wayfinder:** `EngineRuntime::Initialise()` currently creates `SceneRenderExtractor` with access to `Renderer` internals. In v2, `SceneRenderExtractor` bridges app-scoped rendering and state-scoped simulation -- it straddles two lifetimes. Making it purely app-scoped means it initialises without a world; making it state-scoped means it's created/destroyed on every state transition.

**Consequences:**
- Circular dependency between app and state subsystem scopes
- `SceneRenderExtractor` initialised before any world exists, then needs late-binding to the world
- Shutdown order uncertainty: does the extractor detach from the world before or after the world is destroyed?

**Warning signs:**
- Null world pointer in render extraction after a state transition
- Assertion failures during `GameplayState::OnExit` when the extractor tries to query a destroyed world

**Prevention:**
1. `SceneRenderExtractor` should be a state-scoped subsystem (`StateSubsystem`) that requires `Capability::Simulation` and `Capability::Rendering`. Created when `GameplayState` enters, destroyed when it exits.
2. Alternatively, make it app-scoped but with explicit `AttachWorld()`/`DetachWorld()` lifecycle methods (not init-time configuration). The v2 capability system can gate whether extraction runs.
3. Map every current `EngineRuntime` member to its correct v2 scope before starting decomposition:
   - **App-scoped**: Window, Input, Time, RenderDevice, Renderer, BlendableEffectRegistry
   - **State-scoped** (or late-bound): SceneRenderExtractor
4. Draw the dependency graph on paper. If it has a cycle, the decomposition is wrong.

**Phase mapping:** EngineRuntime decomposition phase. Must be designed before implementation.

**CONCERNS.md connection:** "EngineRuntime decomposition" listed as HIGH complexity with dependency on AppSubsystem scope.

---

### Pitfall 5: Capability Set Empty During State Transitions

**What goes wrong:** During a flat transition (State A exits, State B enters), there is a moment where State A's capabilities have been removed but State B's haven't been added yet. If any code queries the effective capability set during this window, capability-gated subsystems appear inactive, overlays deactivate and reactivate causing visual flicker, and render features drop frames.

**Why it happens in Wayfinder:** The v2 design says "effective capability set = app capabilities + active state capabilities." During transition, the state capabilities are briefly empty (between `OnExit(A)` removing A's capabilities and `OnEnter(B)` adding B's). If overlay/subsystem deactivation is triggered eagerly on capability change, everything flashes off and on.

**Consequences:**
- `StateSubsystem`s destroyed at exit, then immediately recreated at enter (unnecessary resource churn)
- Overlays flicker (detach then reattach in the same frame)
- Render features reset their state (pipeline recompilation, cache invalidation)
- Side effects in `OnDetach`/`OnAttach` fire unnecessarily

**Warning signs:**
- Brief visual corruption during state transitions
- Subsystem `Initialise()`/`Shutdown()` called more than expected during transitions
- Log spam: "Overlay deactivated" -> "Overlay activated" in consecutive lines

**Prevention:**
1. Batch the capability transition: compute the **target** capability set before tearing down the old state. Only fire activation/deactivation deltas after both exit and enter are complete.
2. The state machine should have a clear transition protocol:
   ```
   1. Compute new capability set from State B's declarations
   2. OnExit(State A). State A's subsystems/overlays still active.
   3. Destroy State A-only state subsystems (those not in B's set)
   4. Create State B-only state subsystems
   5. OnEnter(State B)
   6. Fire capability delta (deactivate removed, activate added)
   ```
3. Test: transition between two states with overlapping capabilities, verify shared subsystems are not restarted.

**Phase mapping:** ApplicationStateMachine implementation phase. Must be designed into the transition protocol, not patched later.

---

### Pitfall 6: Frame Loop Switchover Breaks Running Functionality

**What goes wrong:** The v1 frame loop in `Application::Loop()` has a specific ordering: SDL events -> EventQueue drain -> LayerStack update -> Game update -> BeginFrame -> RenderScene -> EndFrame. The v2 loop replaces this with: events -> overlays (events) -> active state (events) -> active state (update) -> overlays (update) -> render. If the switchover isn't atomic -- if there's a commit where the old loop is partially dismantled but the new one isn't complete -- the engine enters a state where neither loop is coherent.

**Why it happens in Wayfinder:** The migration plan allows "Journey can break temporarily during structural phases," but specifies "tests pass at phase boundaries." The frame loop touches *everything*: event dispatch, simulation, rendering. There's no way to gradually migrate it -- the old and new loops would fight over who drives the frame.

**Consequences:**
- Events dispatched to both LayerStack and active state (double-handled)
- Rendering called out of order (BeginFrame/EndFrame mismatch)
- Game update runs but render doesn't pick up results (or vice versa)
- Tests that depend on frame ordering break silently

**Warning signs:**
- Render output doesn't reflect the latest simulation update
- Events handled twice (double key presses, double button clicks)
- SDL_GPU validation errors from out-of-order render calls

**Prevention:**
1. The frame loop switchover must be a single commit. Write the entire v2 loop in a new method (`Application::LoopV2()`), test it, then swap the call in `Run()`.
2. Alternatively, use a feature flag: `if (m_useV2Loop) LoopV2(); else Loop();` during the transition. Remove the flag and old loop in the same phase.
3. Do NOT try to "gradually" migrate the loop by replacing pieces. It's inherently atomic.
4. The v2 loop must produce identical visual output to the v1 loop for the Journey sandbox as an integration check.

**Phase mapping:** Frame loop migration phase (late in the milestone, after state machine and overlays exist).

---

## Moderate Pitfalls

Mistakes that cause significant debugging time or require multi-file fixes.

---

### Pitfall 7: Plugin Build() Order Makes Registrations Non-Deterministic

**What goes wrong:** Multiple plugins call `AppBuilder::AddState<T>()`, `RegisterOverlay<T>()`, etc. If validation (duplicate detection, initial state, transition graph) happens during `Build()`, the results depend on plugin call order. If validation is deferred to `AppBuilder::Finalise()`, all registrations are collected first -- but plugins that call `Finalise()`-dependent queries during `Build()` get incomplete data.

**Why it happens in Wayfinder:** The current `PluginRegistry` already defers some work (`ApplyToWorld` happens after all `Build()` calls). But `StateRegistrar` validates duplicate state names during registration. If a v2 plugin registers a state that another plugin also registers, the error depends on who ran first.

**Consequences:**
- Different error messages depending on plugin order
- Missing transitions: Plugin A registers State1->State2, Plugin B adds State2 but registers after A, so the transition is validated before State2 exists

**Prevention:**
1. `AppBuilder::Build()` must be a pure accumulation phase -- no validation, no cross-referencing. All validation happens in `Finalise()`.
2. `Finalise()` returns `Result<AppDescriptor, ValidationErrors>` with all errors collected (not fail-fast on the first).
3. Test: register the same states and transitions in different orders, verify identical `AppDescriptor` output.

**Phase mapping:** AppBuilder implementation phase.

---

### Pitfall 8: Type-Erased ServiceProvider Loses Type Safety at Scale

**What goes wrong:** `StandaloneServiceProvider` uses `std::unordered_map<std::type_index, void*>` with `static_cast<T*>` on retrieval. If a service is registered as type `A` but retrieved as type `B`, the cast is UB. The compiler emits no warning. This is harmless at small scale but becomes a maintenance issue as the number of registered services grows and registrations are scattered across plugin `Build()` calls.

**Why it happens in Wayfinder:** The `ServiceProvider` concept only constrains syntax (`Get<T>()` returns `T&`), not semantics. `StandaloneServiceProvider` is type-erased because it needs to store arbitrary types without knowing them at compile time. The cast from `void*` back to `T*` is correct only if the exact same `typeid(T)` was used for registration.

**Consequences:**
- Silent UB if a derived class is registered but the base class is requested (different `type_index`)
- Memory corruption that manifests far from the registration site
- Particularly dangerous in test code where services are set up ad-hoc

**Warning signs:**
- Mysterious crashes in `Simulation::Initialise()` that only happen in certain test fixtures
- ASAN reports on `static_cast` in `Get<T>()`

**Prevention:**
1. Add a debug-mode type check: store `typeid(T).name()` alongside the `void*`, and in `Get<T>()`, assert that the stored type name matches the requested type name.
2. Prefer `EngineContextServiceProvider` (which delegates to the real `EngineContext` with proper subsystem types) over `StandaloneServiceProvider` everywhere except headless tests.
3. Register services with the exact type they'll be retrieved as. If `Renderer` is registered, `Get<Renderer>()` must be used -- not `Get<IRenderer>()`.
4. Document the constraint: "Register and retrieve with the same type. No base-class lookup."

**Phase mapping:** ServiceProvider implementation phase.

---

### Pitfall 9: Test Fixtures Become Stale During Structural Phases

**What goes wrong:** The 37 test files across 4 test executables depend on v1 APIs (e.g. `SubsystemCollection<GameSubsystem>`, `GameSubsystems::Bind`, `PluginRegistry`, `GameStateMachine`). As these are renamed, restructured, or removed, tests break. If tests aren't updated in the same commit as the API change, there's a window where the test suite doesn't compile, making it impossible to verify whether the structural change broke anything.

**Why it happens in Wayfinder:** Key test dependencies:
- `tests/app/SubsystemTests.cpp`: Tests `SubsystemCollection<GameSubsystem>` directly
- `tests/gameplay/GameStateMachineTests.cpp`: Uses `PluginRegistry`, `GameStateMachine` directly
- `tests/physics/PhysicsTests.cpp` and `PhysicsIntegrationTests.cpp`: Use `GameSubsystems::Bind/Unbind`
- `tests/plugins/PluginRegistryTests.cpp`: Tests `PluginRegistry` API directly

**Consequences:**
- Test compilation failures pile up during structural phases
- Build stays broken for multiple commits, removing the safety net
- Tests get disabled "temporarily" and never re-enabled

**Warning signs:**
- Test executable won't compile after a rename
- `// @todo: re-enable after v2 migration` comments appearing in test files
- Test count decreasing across commits

**Prevention:**
1. Every structural commit must leave all test executables compiling and passing. If a rename changes `GameSubsystem` to `StateSubsystem`, the tests must be updated in the same commit.
2. For major API reshaping (removing `PluginRegistry`, introducing `AppBuilder`): write the new API's tests first, then implement the API, then delete the old tests with the old API. Never a commit where neither test exists.
3. Use `ctest --preset test` as a gate after every structural change.
4. If a test needs to be rewritten rather than updated (e.g. `GameStateMachineTests` testing a concept that no longer exists), the replacement test must exist before the old one is removed.

**Phase mapping:** Every phase. Not a separate step -- it's a constraint on every structural change.

**CONCERNS.md connection:** ~3000 LOC of tests, 37 test files, 4 executables identified.

---

### Pitfall 10: LayerStack Removal Before OverlayStack Is Functional

**What goes wrong:** `LayerStack` currently drives `FpsOverlayLayer` and handles event propagation to layers. If `LayerStack` is removed before `OverlayStack` and `IOverlay` are implemented and wired into the frame loop, the FPS counter disappears and there's no layer-based event propagation. This seems minor, but `FpsOverlayLayer` also validates that the frame loop is actually producing frames -- if it stops updating, you've lost your only in-engine frame timing indicator.

**Why it happens in Wayfinder:** The migration plan lists `LayerStack` removal as its own step. But the temptation during "cleanup" phases is to remove dead code early. If `LayerStack` is removed because "nothing important uses it anymore" before overlays exist, the `FpsOverlayLayer`'s functionality disappears.

**Consequences:**
- No visual frame rate indicator during development
- Event propagation to overlays silently stops
- Journey sandbox runs but has no debug UI

**Prevention:**
1. `LayerStack` removal must be the **last** step, after `OverlayStack` is functional and `FpsOverlay` (rewritten from `FpsOverlayLayer`) is rendering.
2. Gate: both old and new overlay must render the same info in the same frame before removing the old one.

**Phase mapping:** Late - after overlay implementation and frame loop switchover.

**CONCERNS.md connection:** Removal listed. Also referenced in v2 prerequisites.

---

### Pitfall 11: Capability System Becomes a Hidden Global Registry

**What goes wrong:** The capability system uses `GameplayTag` values that are defined as `inline const` globals in a `Capability` namespace. Plugins declare new capabilities freely. Without a central registry of what capabilities exist and what they mean, capabilities become a second tag system with no validation -- typos in capability declarations silently create new (unused) capabilities.

**Why it happens in Wayfinder:** `GameplayTag` is string-based (backed by `InternedString`). `Capability.Simulation` and `Capability.Simulaton` (typo) are different tags. The `GameplayTagRegistry` validates tag hierarchies loaded from TOML files, but capability tags declared in code bypass this validation.

**Consequences:**
- A subsystem with `RequiredCapabilities = { GameplayTag("Capability.Simulaton") }` is never activated (typo in the tag name)
- No compile-time or runtime error -- the subsystem just silently doesn't work
- Debugging: "why isn't this subsystem created?" leads to hours of tracing capability flows

**Warning signs:**
- Subsystem or overlay that should be active in a state but isn't
- `TryGetStateSubsystem<T>()` returns nullptr in states where it should be valid

**Prevention:**
1. Define all engine-level capabilities as `inline const GameplayTag` constants in a single header (`Capabilities.h`). Subsystems reference these constants, not string literals.
2. In `AppBuilder::Finalise()`, validate that every `RequiredCapabilities` tag in any registration matches a tag that some state or plugin provides. Flag unused capability requirements as warnings.
3. Never use string literals for capability tags in engine code. Only `Capability::Simulation`, `Capability::Rendering`, etc.

**Phase mapping:** Capability system implementation phase.

---

### Pitfall 12: EngineContext Becomes a God Object

**What goes wrong:** `EngineContext` is designed as the "central service-access mechanism" replacing static globals. If it grows to offer `GetAppSubsystem<T>()`, `TryGetAppSubsystem<T>()`, `GetStateSubsystem<T>()`, `TryGetStateSubsystem<T>()`, `RequestTransition<T>()`, `RequestPush<T>()`, `RequestPop()`, `ActivateOverlay()`, `DeactivateOverlay()`, `GetProjectDescriptor()`, `GetAppDescriptor()`, and more -- it becomes the new monolith. Every system takes `EngineContext&` as its first parameter, and the context provides access to everything, defeating the purpose of the decomposition.

**Why it happens in Wayfinder:** The v2 plan explicitly makes `EngineContext` the replacement for `EngineRuntime` monolith. The difference is supposed to be that `EngineContext` doesn't *own* anything -- it's a query facade. But the API surface can still grow unbounded.

**Consequences:**
- Every function signature starts with `EngineContext& ctx` -- code reads like EngineRuntime never left
- Testing requires constructing a full `EngineContext` even for isolated subsystem tests
- Adding a new subsystem means touching `EngineContext` (if it uses special-case accessors)

**Warning signs:**
- `EngineContext.h` growing past ~100 lines
- Test helpers constructing mock EngineContexts with many null subsystems
- Subsystems accessing `EngineContext` to reach other subsystems they shouldn't know about

**Prevention:**
1. `EngineContext` is a thin facade -- `GetSubsystem<T>()` and `RequestTransition<T>()` only. No domain-specific convenience methods.
2. Subsystems that need other subsystems declare `DependsOn` and receive them during init, not via context queries at runtime.
3. `ServiceProvider` concept for `Simulation` is the model -- constrained interface, not a kitchen sink.
4. Code review gate: reject any PR that adds a named accessor to `EngineContext` (all access goes through generic `GetSubsystem<T>()`).

**Phase mapping:** EngineContext implementation phase. Set the pattern early.

**CONCERNS.md connection:** Current `EngineContext` is a 5-field non-owning struct. The v2 version must stay lean.

---

## Minor Pitfalls

Mistakes that add friction but don't block progress.

---

### Pitfall 13: DLL Plugin Dead Code Confuses the Migration

**What goes wrong:** `PluginExport.h`, `PluginLoader.h/cpp`, and `CreateGamePlugin()` are marked for removal but linger during the migration. New code inadvertently references them, or tests still exercise DLL loading paths.

**Prevention:**
1. Remove DLL plugin files early in the migration (first structural phase). Dead code grows roots.
2. Remove `EntryPoint.h`'s `CreateGamePlugin()` extern in the same commit.
3. Update Journey's `main.cpp` to construct `Application` directly with the plugin.

**Phase mapping:** First structural phase. Quick, low-risk, high-clarity win.

**CONCERNS.md connection:** DLL Plugin System section -- "If the transition is partial, the dead code could confuse future contributors."

---

### Pitfall 14: Scene -> Gameplay Layer Violation Deepens During Migration

**What goes wrong:** `ComponentRegistry.cpp` already has improper includes from `gameplay/` (tag registry, subsystem access). During migration, if `BlendableEffectRegistry` becomes an `AppSubsystem` but `ComponentRegistry` still calls `GetActiveInstance()`, the layering violation persists with a new name. Worse, new code might follow the existing pattern ("ComponentRegistry already does it, so it must be fine").

**Prevention:**
1. When `BlendableEffectRegistry` becomes `AppSubsystem`, the component deserialisation code must receive it as a parameter (injected by the scene loader), not query a static/global.
2. Don't defer this to "post-migration cleanup." The migration is the natural point to fix it because the APIs are already changing.
3. Mark the three `GetActiveInstance()` call sites in `ComponentRegistry.cpp` (lines 669, 945, 1185) for explicit attention during the subsystem scoping phase.

**Phase mapping:** Subsystem scoping phase or BlendableEffectRegistry migration.

**CONCERNS.md connection:** "Scene -> Gameplay/Rendering Coupling" section. Three specific includes and three `GetActiveInstance()` call sites identified.

---

### Pitfall 15: Non-Owning Pointer Lifetimes Shift During Scope Changes

**What goes wrong:** Several subsystems cache non-owning raw pointers (e.g. `FpsOverlayLayer::m_window`, `GameStateMachine::m_world`, `Game::m_assetService`). When subsystem scopes change (game-scoped to state-scoped, monolith to app-scoped), the ownership graph changes, and a pointer that was valid for the full app lifetime might now only be valid for a state's lifetime.

**Why it happens in Wayfinder:** `FpsOverlayLayer` holds a `Window&` -- currently fine because `EngineRuntime` (owner) outlives `LayerStack`. In v2, `FpsOverlay` (replacement) will get `Window&` from `EngineContext` -- still fine because Window is app-scoped. But `GameStateMachine::m_world` holds a pointer to the flecs world -- currently fine because `Game` owns both. In v2, the world lives in `Simulation` (state-scoped) while the state machine might be in a different scope.

**Prevention:**
1. For each non-owning pointer in the codebase, verify: does the owner still outlive the consumer in v2?
2. Identified risk points: `GameStateMachine::m_world` and `Game::m_assetService`. Both move to different scopes in v2.
3. Prefer references over raw pointers for state-scoped dependencies -- attached during `OnEnter`, the reference is valid until `OnExit`.

**Phase mapping:** Each subsystem migration phase. Check per-subsystem.

**CONCERNS.md connection:** "Non-owning Raw Pointers" section lists specific pointer risks.

---

### Pitfall 16: Push/Pop State Stack Depth and Leak

**What goes wrong:** The v2 `ApplicationStateMachine` supports unlimited push depth. If a pushed state fails to pop (bug in game logic, forgotten pop path), the stack grows unboundedly. Suspended states below hold onto their resources (subsystems, ECS world state).

**Prevention:**
1. Add a configurable max stack depth (e.g. 8). Assert if exceeded.
2. Log the full state stack on every transition in debug builds.
3. Test: push N states, verify pop returns to the correct state at each level.

**Phase mapping:** ApplicationStateMachine implementation phase.

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Severity | Mitigation |
|-------------|---------------|----------|------------|
| IApplicationState + StateMachine | Re-entrancy during transitions (#1) | Critical | Guard flag, deferred-only transitions |
| Subsystem scoping | Init-order dependencies (#2) | Critical | Topological sort before decomposition |
| EngineRuntime decomposition | Circular scope deps (#4), scope misassignment | Critical | Map scopes first, draw dependency graph |
| Static accessor rename | Flecs callback constraints (#3), Bind/Unbind sites missed | Critical | Atomic rename, keep pattern alive |
| Capability system | Empty set during transitions (#5), typos (#11) | Moderate | Batch transitions, constant-only tags |
| Frame loop migration | Non-atomic switchover (#6) | Critical | Single commit or feature flag |
| AppBuilder | Build-time validation timing (#7) | Moderate | Accumulate-then-validate pattern |
| ServiceProvider | Type safety at scale (#8) | Moderate | Debug assertions, typed constants |
| LayerStack removal | Premature removal (#10) | Moderate | Remove last, after OverlayStack works |
| DLL plugin removal | Lingering dead code (#13) | Minor | Remove first, clean break |
| Every structural phase | Test staleness (#9) | Moderate | Tests compile and pass per commit |
| Scene layer coupling | Deepening violation (#14) | Minor | Fix during migration, not after |

---

## Sources

All findings are grounded in direct codebase analysis of:
- [engine/wayfinder/src/app/](engine/wayfinder/src/app/) - Application, EngineRuntime, Subsystem, LayerStack
- [engine/wayfinder/src/gameplay/](engine/wayfinder/src/gameplay/) - Game, GameStateMachine, GameplayTagRegistry
- [engine/wayfinder/src/scene/ComponentRegistry.cpp](engine/wayfinder/src/scene/ComponentRegistry.cpp) - Static accessor usage
- [engine/wayfinder/src/volumes/BlendableEffectRegistry.cpp](engine/wayfinder/src/volumes/BlendableEffectRegistry.cpp) - Static instance pattern
- [docs/plans/application_architecture_v2.md](docs/plans/application_architecture_v2.md) - v2 design
- [docs/plans/application_migration_v2.md](docs/plans/application_migration_v2.md) - Migration tables
- [docs/plans/game_framework.md](docs/plans/game_framework.md) - Simulation/ServiceProvider design
- [tests/](tests/) - Test structure and fixture patterns
- `.planning/codebase/CONCERNS.md` - Known technical debt

Confidence: HIGH. All pitfalls reference specific files, line numbers, or documented design decisions. No external research needed -- this is domain-specific analysis of the actual codebase and migration plan.
