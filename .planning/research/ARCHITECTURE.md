# Architecture Patterns: Monolithic-to-Modular Engine Migration

**Domain:** C++23 game engine application architecture migration
**Researched:** 2026-04-03
**Confidence:** HIGH (analysis based on actual codebase inspection + established migration patterns)

---

## Executive Summary

This migration restructures ~7500 LOC of engine code from a monolithic application shell (EngineRuntime + LayerStack + Game) to a plugin-composed architecture (EngineContext + ApplicationStateMachine + OverlayStack + Simulation). The codebase is small enough for a controlled strangler fig approach but large enough to benefit from incremental migration rather than a big-bang rewrite.

The critical insight: the v2 architecture has a clear dependency DAG that dictates build order. Foundation types (interfaces, base classes, EngineContext) must exist before anything that uses them. The migration decomposes into roughly four tiers: (1) foundation types with no dependencies, (2) subsystem infrastructure that depends on foundation, (3) state machine and overlay systems that compose subsystems, and (4) integration/wiring that connects everything and removes v1 code.

---

## Migration Strategy: Controlled Strangler Fig

### Why Not Big-Bang

A big-bang rewrite (build all v2, then swap) is tempting at 7500 LOC but has critical downsides:

- **No intermediate validation.** Tests can't run against half-built systems. Bugs compound across all changes.
- **Merge conflicts.** If any bug fixes or features must land during the rewrite, they conflict with everything.
- **No morale checkpoints.** Seven thousand lines of untestable rework with no green builds is demoralising.

### Why Not Pure Strangler Fig

A pure strangler fig (route traffic to new system gradually) works for services but is awkward for a single-process engine where the frame loop is a tightly coupled pipeline. You can't route "half a frame" to v2.

### Recommended: Tiered Incremental Migration

Build v2 components bottom-up alongside v1. Each tier is testable in isolation. Once a tier is complete and tested, the next tier can wire into it. v1 components are removed only after their v2 replacements are integrated and tested.

**Key property:** Journey (the sandbox) can break during structural phases, but tests must pass at each phase boundary. This is explicitly permitted by the project constraints.

---

## Component Dependency Graph

The v2 components form a strict dependency DAG. Building out of order creates unnecessary scaffolding or breaks compilation.

```
Tier 0 (Foundation - no v1 dependencies)
  IApplicationState          (pure interface)
  IOverlay                   (pure interface)
  IPlugin                    (pure interface, replaces Plugin)
  IStateUI                   (pure interface)
  ServiceProvider concept    (concept + StandaloneServiceProvider)
  StateMachine<TStateId>     (generic template, no engine deps)
  AppSubsystem base class    (extends Subsystem, no engine deps)
  StateSubsystem base class  (extends Subsystem, rename from GameSubsystem)
  Capability tags            (GameplayTag constants, trivial)

Tier 1 (Infrastructure - depends on Tier 0)
  SubsystemRegistry<TBase>   (extends SubsystemCollection with dep ordering + capabilities)
  EngineContext (v2)          (owns SubsystemRegistries, provides service access)
  AppBuilder                  (replaces PluginRegistry, depends on IPlugin + registrars)
  AppDescriptor               (read-only snapshot from AppBuilder::Finalise())

Tier 2 (Orchestration - depends on Tier 1)
  ApplicationStateMachine     (depends on IApplicationState + EngineContext)
  OverlayStack                (depends on IOverlay + EngineContext + capabilities)
  Simulation                  (depends on ServiceProvider, replaces Game)

Tier 3 (Concrete States + Integration)
  GameplayState               (depends on Simulation + ApplicationStateMachine + EngineContext)
  EditorState (stub)          (depends on Simulation + ApplicationStateMachine)
  FpsOverlay                  (depends on IOverlay + EngineContext)
  EngineRuntime decomposition (Window/Input/Time/Renderer become AppSubsystems)
  Application rewrite         (wires everything together, removes v1 code)
  Journey update              (sandbox uses new architecture)

Tier 4 (Cleanup)
  Remove LayerStack, Layer, FpsOverlayLayer
  Remove PluginExport.h, PluginLoader.h/cpp
  Remove EngineRuntime (monolith)
  Remove old EngineContext struct
  Remove Game class
  Remove DLL plugin system (CreateGamePlugin, EntryPoint.h rewrite)
```

### Why This Order

1. **Tier 0 first** because these types have zero dependencies and can be tested in pure unit tests immediately. They form the vocabulary every subsequent tier speaks.

2. **Tier 1 second** because the subsystem infrastructure is the spine of v2. Every state, overlay, and the Application itself interacts with EngineContext and SubsystemRegistry. Getting these right and tested before building consumers prevents cascading rewrites.

3. **Tier 2 third** because the state machine and overlay stack are the primary consumers of the infrastructure but also producers that the concrete states consume. Simulation must exist before GameplayState can wrap it.

4. **Tier 3 is integration** - concrete types that prove the architecture works end-to-end. EngineRuntime decomposition happens here (not earlier) because the AppSubsystem instances need the SubsystemRegistry infrastructure from Tier 1 to exist.

5. **Tier 4 is cleanup** - only after all v2 paths are tested and working do we remove v1 code. This is the "strangle" step.

---

## Component Boundaries

### What Can Be Migrated Independently

These components have no coupling to each other during construction and can be built/tested in any order within their tier:

| Component | Dependencies | Test Strategy |
|-----------|-------------|---------------|
| `IApplicationState` | None | Interface only, test via mock implementations |
| `IOverlay` | None | Interface only, test via mock implementations |
| `IPlugin` | None | Interface only |
| `ServiceProvider` concept | None | Test with StandaloneServiceProvider |
| `StateMachine<TStateId>` | None | Pure unit tests with InternedString |
| `Capability` tags | `GameplayTag` (exists) | Trivial constants |
| `AppSubsystem` | `Subsystem` (exists) | Base class, tested through SubsystemRegistry |
| `StateSubsystem` | `Subsystem` (exists) | Rename from GameSubsystem |

### What Must Move Together

These components have hard mutual dependencies and should be built in the same phase or immediately adjacent phases:

| Cluster | Components | Why Coupled |
|---------|-----------|-------------|
| **Subsystem Infrastructure** | `SubsystemRegistry<TBase>` + `EngineContext` (v2) | EngineContext owns the registries; they're useless without each other |
| **Plugin Wiring** | `IPlugin` + `AppBuilder` + `AppDescriptor` | AppBuilder is the IPlugin's interaction surface; AppDescriptor is its output |
| **State Machine + States** | `ApplicationStateMachine` + `GameplayState` + EngineRuntime decomposition | The state machine needs at least one real state to prove it works; GameplayState needs subsystems as AppSubsystems |
| **Simulation Migration** | `Simulation` + `ServiceProvider` + `GameplayState` | Simulation is consumed by GameplayState; ServiceProvider is its access mechanism |
| **Application Rewrite** | New `Application::Initialise()` + `Application::Loop()` + all removal | Must happen atomically - can't half-wire the frame loop |

### Coupling Risks During Migration

| v1 Component | v2 Dependencies | Risk |
|-------------|----------------|------|
| `Application::Initialise()` | Creates EngineRuntime, Game, LayerStack in sequence | Must be the last thing rewritten - everything it creates must have a v2 replacement |
| `Application::Loop()` | Calls EngineRuntime, LayerStack, Game in tight sequence | Can't partially migrate the loop; the whole frame must switch at once |
| `Game` | Owns flecs world, subsystems, state machine, scene | Must decompose in order: subsystems out first, then state machine, then Game -> Simulation |
| `EngineRuntime` | Owns Window, Input, Time, RenderDevice, Renderer as sequential creates | Can decompose to AppSubsystems one at a time if SubsystemRegistry exists |

---

## Data Flow During Migration

### Dual-System Coexistence

During the transition, v1 and v2 systems must coexist. The key principle: **v2 types are built and tested alongside v1, but v1 runs the show until the final integration phase.**

```
Phase: Building Tier 0-2
  v1 Application owns the frame loop (unchanged)
  v2 types exist in engine/wayfinder/src/ but are not wired into Application
  Tests exercise v2 types directly (unit tests, no Application needed)

Phase: Building Tier 3
  v1 Application::Initialise() is gradually replaced
  EngineRuntime services move to AppSubsystems one at a time
  Game::Initialise() logic moves to GameplayState::OnEnter()
  
Phase: Integration
  Application switches to v2 frame loop
  Journey updated to use AppBuilder + plugin composition
  v1 code paths become dead code

Phase: Cleanup
  Dead v1 code removed
  Tests that tested v1 code are rewritten for v2
```

### Data Flow: v1 (current)

```
[ProjectDescriptor] -> [EngineConfig] -> [PluginRegistry]
                                              |
                                    [EngineRuntime::Initialise()]
                                              |
                                      [Game::Initialise()]
                                              |
                                         Frame Loop:
SDL Events -> EventQueue -> LayerStack -> Game::Update -> ECS progress
                                       -> EngineRuntime::BeginFrame
                                       -> Renderer::RenderScene
                                       -> EngineRuntime::EndFrame
```

### Data Flow: v2 (target)

```
[ProjectDescriptor] -> [AppBuilder (per-plugin Build())]
                              |
                       [AppDescriptor = AppBuilder::Finalise()]
                              |
                  [EngineContext::Initialise(AppDescriptor)]
                       |                    |
              [AppSubsystems created]  [Capability set computed]
                              |
                  [ApplicationStateMachine::Start()]
                              |
                   [Initial IApplicationState::OnEnter()]
                              |
                    [StateSubsystems created based on capabilities]
                              |
                         Frame Loop:
SDL Events -> EventQueue -> OverlayStack (top-down) -> ActiveState::OnEvent
                         -> ActiveState::OnUpdate -> Simulation::Update
                         -> OverlayStack::OnUpdate
                         -> ActiveState::OnRender -> Scene submission
                         -> OverlayStack::OnRender
```

### Key Data Flow Changes

| Aspect | v1 | v2 |
|--------|-----|-----|
| **Plugin interaction surface** | `PluginRegistry` (passive store) | `AppBuilder` (active builder with validation) |
| **Service access** | `EngineRuntime::GetWindow()` etc. | `EngineContext::GetAppSubsystem<Window>()` |
| **Game state access** | `GameSubsystems::Get<T>()` static | `EngineContext::GetStateSubsystem<T>()` |
| **Frame lifecycle** | LayerStack drives update/render | ApplicationStateMachine drives update/render |
| **Event routing** | LayerStack top-down | OverlayStack top-down, then active state |
| **Subsystem lifetime** | Game owns all | App-scoped (EngineContext) + State-scoped (EngineContext, gated) |
| **ECS world ownership** | Game | Simulation (created by GameplayState::OnEnter) |

---

## Suggested Build Order and Phase Structure

Based on the dependency graph and coupling analysis, here is the recommended phase breakdown. Each phase produces testable output.

### Phase 1: Foundation Types (Tier 0)

**Build:** `IApplicationState`, `IOverlay`, `IPlugin`, `IStateUI`, `ServiceProvider` concept, `StandaloneServiceProvider`, `StateMachine<TStateId>`, `AppSubsystem`, `StateSubsystem` (rename), capability tag constants.

**Test:** Unit tests for `StateMachine<TStateId>` (transitions, callbacks, deferred processing, invalid transitions). Unit tests for `StandaloneServiceProvider` (register, get, try-get, missing service). Mock implementations of interfaces to verify signatures compile.

**v1 impact:** None. All new files. `GameSubsystem` renamed to `StateSubsystem` with a `using` alias for temporary backward compatibility.

**Why first:** Zero dependencies. Establishes the vocabulary for all subsequent work. Can be tested with pure unit tests (no engine infrastructure needed). Fast to build, high confidence.

**Removes:** Nothing yet.

### Phase 2: Subsystem Infrastructure (Tier 1)

**Build:** `SubsystemRegistry<TBase>` (extends SubsystemCollection with `DependsOn`, topological sort, capability-based activation). `EngineContext` v2 class (owns AppSubsystemRegistry + StateSubsystemRegistry, typed getters, deferred transition/overlay requests).

**Test:** SubsystemRegistry tests: dependency ordering, capability filtering, init/shutdown order, circular dependency detection. EngineContext tests: subsystem registration, lookup, state subsystem lifecycle (create on state enter, destroy on state exit).

**v1 impact:** Minimal. SubsystemCollection still exists, used by v1 code. New SubsystemRegistry is a separate type. Old EngineContext struct coexists with new EngineContext class (different header, different namespace or name during transition).

**Why second:** EngineContext is the spine of v2. Every state, overlay, and subsystem interacts with it. Must be solid before building consumers.

**Removes:** Nothing yet.

### Phase 3: Plugin and Builder (Tier 1)

**Build:** `AppBuilder` (replaces PluginRegistry internals; typed registrar store, state registration, overlay registration, capability declarations, ForState<T>(), LoadConfig<T>()). `AppDescriptor` (read-only snapshot from Finalise()). Refactor existing registrars to work with AppBuilder.

**Test:** AppBuilder tests: plugin Build() is called, registrars populated, Finalise() produces valid AppDescriptor, duplicate detection, transition validation. Integration test: existing registrar tests adapted for AppBuilder surface.

**v1 impact:** Medium. PluginRegistry can delegate to AppBuilder internally (adapter pattern) or AppBuilder can be built alongside. Existing Plugin::Build(PluginRegistry&) still works during transition.

**Why third:** AppBuilder is the plugin's interaction surface. Must exist before any concrete state or overlay can be registered.

**Removes:** Nothing yet (PluginRegistry still used by v1 Application).

### Phase 4: Simulation and Orchestration (Tier 2)

**Build:** `Simulation` (rename/refactor Game: strip state machine ownership, strip subsystem ownership, add ServiceProvider-based init). `ApplicationStateMachine` (flat + push/pop, transition validation, deferred transitions, lifecycle dispatch). `OverlayStack` (ordered stack, capability filtering, activation/deactivation).

**Test:** Simulation tests: headless init via StandaloneServiceProvider, update, scene load, world access. ApplicationStateMachine tests: flat transitions, push/pop, lifecycle callbacks (enter/exit/suspend/resume), transition validation, deferred processing. OverlayStack tests: ordering, capability gating, event dispatch order.

**v1 impact:** Game class still exists but is being superseded. Simulation tests use StandaloneServiceProvider (no v1 dependencies).

**Why fourth:** These are the primary orchestration types. Simulation needs ServiceProvider (Phase 1). ApplicationStateMachine needs IApplicationState (Phase 1) and EngineContext (Phase 2). OverlayStack needs IOverlay (Phase 1) and capabilities (Phase 1).

**Removes:** Nothing yet.

### Phase 5: Concrete States and Engine Decomposition (Tier 3)

**Build:** `GameplayState` (wraps Simulation into IApplicationState lifecycle, owns sub-state machine, IStateUI wiring). `EditorState` stub (proves IApplicationState pattern). `FpsOverlay` (rewrite of FpsOverlayLayer as IOverlay). Decompose EngineRuntime: Window, Input, Time, Renderer each become AppSubsystem instances with proper lifecycle.

**Test:** GameplayState tests: enter creates Simulation, exit destroys it, sub-state machine wiring, IStateUI lifecycle. EditorState tests: enter/exit lifecycle. FpsOverlay tests: attach/detach, render calls. AppSubsystem integration: dependency ordering, capability gating for Renderer (needs GPU capability).

**v1 impact:** High. EngineRuntime decomposition directly affects how services are accessed. This is the phase where the frame loop must transition.

**Why fifth:** Concrete states need Simulation (Phase 4), ApplicationStateMachine (Phase 4), and EngineContext (Phase 2). Engine decomposition needs SubsystemRegistry (Phase 2).

**Removes:** Nothing yet (v1 code still present but no longer on the active path).

### Phase 6: Integration and Application Rewrite (Tier 3)

**Build:** Rewrite `Application::Initialise()` to use AppBuilder -> EngineContext -> ApplicationStateMachine flow. Rewrite `Application::Loop()` to use v2 frame lifecycle. Update Journey sandbox to use new architecture (AppBuilder, plugin composition, game owns main()).

**Test:** Integration tests: full Application init with v2 path. Journey compiles and runs. All existing tests pass (rewritten where needed).

**v1 impact:** Maximum. This is the switchover. The v1 frame loop is replaced.

**Why sixth:** Everything v2 must exist and be tested before the Application can be rewritten.

**Removes:** v1 Application::Initialise() and Loop() code paths.

### Phase 7: Cleanup (Tier 4)

**Build:** Nothing new.

**Remove:** `LayerStack`, `Layer`, `FpsOverlayLayer`, `PluginExport.h`, `PluginLoader.h/cpp`, old `EngineRuntime` class, old `EngineContext` struct, `Game` class, `GameContext` struct, `GameSubsystem` alias (if still present), `BackendConfig`/`PlatformBackend`/`RenderBackend` enums, `CreateGamePlugin()`, `EntryPoint.h` old form.

**Test:** All tests pass. No references to removed types. Clean compilation.

**Why last:** Removal is safe only after all consumers are migrated. Removing too early creates compilation failures that block other work.

---

## Testing Strategy Per Migration Stage

### Principles

1. **Test v2 types in isolation first.** Don't wire into v1 Application to test a new interface.
2. **StandaloneServiceProvider is the testing backbone.** It lets Simulation and subsystems be tested headless without EngineContext.
3. **Existing v1 tests remain green** until Phase 6 (integration) when they're rewritten.
4. **Phase boundary invariant:** all test executables compile and pass at the end of each phase.

### Test Coverage Per Phase

| Phase | New Tests | Modified Tests | Test Type |
|-------|-----------|----------------|-----------|
| 1: Foundation | StateMachine, ServiceProvider, interface compile checks | None | Pure unit |
| 2: Subsystem Infra | SubsystemRegistry (ordering, capabilities), EngineContext (lookup, scoping) | SubsystemTests.cpp (extend or parallel) | Unit |
| 3: Plugin/Builder | AppBuilder (registration, validation, Finalise) | PluginRegistryTests.cpp adapted | Unit |
| 4: Simulation/Orchestration | Simulation (headless), ApplicationStateMachine (lifecycle), OverlayStack | GameStateMachineTests.cpp influenced | Unit + integration |
| 5: Concrete States | GameplayState lifecycle, FpsOverlay, AppSubsystem integration | EngineRuntimeTests.cpp rewritten | Integration |
| 6: Application Rewrite | Full Application init/loop with v2 | All tests updated for v2 API | End-to-end |
| 7: Cleanup | Verify nothing references removed types | Remove obsolete test code | Compilation |

### What Tests to Write When

**Before implementation (TDD appropriate):**
- `StateMachine<TStateId>` - pure logic, well-defined state space
- `SubsystemRegistry` dependency ordering - algorithmic, edge cases matter
- `ApplicationStateMachine` transition validation - complex state space

**Alongside implementation:**
- `EngineContext` subsystem access - API design evolves during implementation
- `AppBuilder` registration - interface settles during first plugin port
- `Simulation` headless init - integration with flecs world

**After implementation (verification):**
- `GameplayState` full lifecycle - needs Simulation + EngineContext to be stable
- Application integration - needs everything else to be done
- Journey smoke test - end-to-end validation

---

## Integration Points: v1/v2 Coexistence

### Phase 1-3: Clean Separation

v2 types exist as new files. v1 code is untouched. No integration needed. Test in isolation.

```
engine/wayfinder/src/app/
  Application.cpp          (v1, unchanged)
  EngineRuntime.h          (v1, unchanged)
  IApplicationState.h      (v2, NEW)
  IOverlay.h               (v2, NEW)
  StateMachine.h           (v2, NEW)
  SubsystemRegistry.h      (v2, NEW, extends SubsystemCollection.h)
  EngineContextV2.h        (v2, NEW, coexists with EngineContext.h)
  AppBuilder.h             (v2, NEW, coexists with PluginRegistry.h)
```

### Phase 4: Simulation/Game Coexistence

`Simulation` is built as a separate class alongside `Game`. They share the same flecs world patterns but have different ownership models. Tests use `StandaloneServiceProvider` - no conflict with v1.

**Temporary bridge:** If needed during Phase 5-6, `Game` can delegate to `Simulation` internally (adapter pattern) so that v1 Application still works while v2 states are being wired.

### Phase 5: EngineRuntime Decomposition

This is the riskiest integration point. Services must be accessible both via `EngineRuntime::GetWindow()` (v1) and `EngineContext::GetAppSubsystem<Window>()` (v2) during transition.

**Recommended approach:**
1. Create `WindowSubsystem : AppSubsystem` that wraps Window creation/lifecycle
2. Register it in SubsystemRegistry
3. EngineRuntime delegates to SubsystemRegistry internally (gets services from there)
4. v1 accessors (`EngineRuntime::GetWindow()`) still work but point to the same instances
5. Once Application is rewritten, EngineRuntime wrapper is removed

### Phase 6: The Switchover

The frame loop must switch atomically. Both loops can exist in code (behind a flag or as a complete rewrite), but only one runs. Recommended: rewrite the functions directly and remove the old code in the same phase.

```cpp
// Phase 6: Application::Initialise() is completely rewritten
// Old: EngineRuntime -> Game -> LayerStack
// New: AppBuilder -> EngineContext -> ApplicationStateMachine

// Phase 6: Application::Loop() is completely rewritten  
// Old: BeginFrame -> LayerStack -> Game::Update -> RenderScene -> EndFrame
// New: ProcessTransitions -> Events -> ActiveState -> Overlays -> Render
```

---

## Patterns to Follow

### Pattern: Bottom-Up Construction with Top-Down Removal

Build v2 leaf types first (interfaces, base classes), then infrastructure, then orchestration, then concrete types. Remove v1 types in the opposite direction: concrete types first (LayerStack, EngineRuntime), then infrastructure (PluginRegistry), then base types (Layer, Plugin).

### Pattern: Adapter Bridging During Transition

When v1 and v2 must coexist, use adapters rather than modifying v1 types:

```cpp
// Bridge: EngineRuntime delegates to AppSubsystems
class EngineRuntime
{
    Window& GetWindow() { return m_context.GetAppSubsystem<Window>(); }
};
```

### Pattern: Test-First Foundation Types

For Tier 0 types (StateMachine, ServiceProvider, interfaces), write tests before implementation. The API surface is well-defined by the planning documents. Tests validate the contract; implementation fills it.

### Pattern: Capability Tags as Feature Flags

During migration, capabilities can gate whether v2 subsystems activate. An app-level capability `Capability.Migration.V2Subsystems` could control whether the new subsystem registry is active, with fallback to v1 SubsystemCollection.

---

## Anti-Patterns to Avoid

### Anti-Pattern: Modifying v1 to Support v2

Don't add v2 capabilities to v1 types (e.g., adding capability support to LayerStack). Build v2 types separately and switch over. Modifying v1 creates a hybrid that's harder to test and harder to remove.

### Anti-Pattern: Top-Down Migration

Don't start by rewriting Application and working downward. Application is the last thing to rewrite because it depends on everything else existing.

### Anti-Pattern: Premature v1 Removal

Don't remove LayerStack before ApplicationStateMachine + OverlayStack are proven. Don't remove EngineRuntime before AppSubsystems are wired. Dead code is temporarily acceptable; broken builds are not.

### Anti-Pattern: Shared Mutable State During Transition

Don't have v1 and v2 code paths writing to the same mutable state. If EngineRuntime and AppSubsystems both manage Window, ensure one delegates to the other rather than both owning Window lifecycle.

---

## v1 Component Removal Schedule

| Phase | Components Removable | Gate |
|-------|---------------------|------|
| After Phase 4 | None (v1 still runs the show) | -- |
| After Phase 5 | `FpsOverlayLayer` (replaced by FpsOverlay) | FpsOverlay tested |
| After Phase 6 | `LayerStack`, `Layer`, `EngineRuntime`, `Game`, `GameContext`, v1 `EngineContext`, `PluginRegistry` internals, `PluginExport.h`, `PluginLoader.h/cpp`, `Plugin` base class, `EntryPoint.h` | Application uses v2 path, Journey updated |
| Phase 7 | All remaining dead code, backward-compat aliases | All tests green on v2 |

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| EngineRuntime decomposition breaks rendering | Medium | High | Keep EngineRuntime as adapter during transition; decompose one service at a time |
| ApplicationStateMachine lifecycle complexity | Medium | Medium | Exhaustive unit tests for state transitions before wiring to real states |
| SubsystemRegistry ordering bugs | Low | High | Reuse SystemRegistrar's proven Kahn's algorithm; extensive ordering tests |
| Flecs world ownership ambiguity | Low | Medium | Simulation owns world, GameplayState owns Simulation - clear single owner |
| Test rewrite volume at Phase 6 | High | Low | Tests are rewritten, not discarded. v2 tests built alongside in Phases 1-5 reduce Phase 6 load |
| Journey breakage duration | Medium | Low | Explicitly permitted. Phase 6 restores Journey. Other test executables remain green throughout |

---

## Sources

- Codebase inspection: `engine/wayfinder/src/app/`, `engine/wayfinder/src/gameplay/`, `engine/wayfinder/src/plugins/`
- Planning documents: `docs/plans/application_architecture_v2.md`, `docs/plans/application_migration_v2.md`, `docs/plans/game_framework.md`
- Architecture mapping: `.planning/codebase/ARCHITECTURE.md`, `.planning/codebase/CONCERNS.md`
- Migration pattern references: Martin Fowler's Strangler Fig pattern, Michael Feathers' "Working Effectively with Legacy Code" seam identification approach
- Confidence: HIGH - analysis derived entirely from inspected codebase and existing planning documents, with established software engineering migration patterns applied
