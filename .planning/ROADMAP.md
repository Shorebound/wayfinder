# Roadmap: Wayfinder Application Architecture v2 Migration

## Overview

This milestone migrates Wayfinder's application shell from a monolithic design (EngineRuntime + LayerStack + Game) to the v2 plugin-composed architecture (EngineContext + ApplicationStateMachine + OverlayStack + Simulation). The migration follows the dependency DAG bottom-up: foundation types first, then infrastructure, orchestration, concrete integration, and finally cleanup of dead v1 code. Each phase is independently testable; v1 code remains functional until the atomic frame loop switchover in Phase 6.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Foundation Types** - V2 interfaces, concepts, base types, and DLL plugin removal
- [ ] **Phase 2: Subsystem Infrastructure** - SubsystemRegistry with dependency ordering and capability-gated activation
- [ ] **Phase 3: Plugin Composition** - AppBuilder, typed registrar store, per-plugin configuration
- [ ] **Phase 4: Orchestration** - ApplicationStateMachine, Simulation, OverlayStack with full lifecycle
- [ ] **Phase 5: Concrete States and Engine Decomposition** - GameplayState, EditorState stub, FpsOverlay, EngineRuntime split into AppSubsystems
- [ ] **Phase 6: Application Rewrite and Integration** - V2 main loop, Journey sandbox, test migration
- [ ] **Phase 7: Cleanup** - Remove all dead v1 code

## Phase Details

### Phase 1: Foundation Types
**Goal**: All v2 vocabulary types exist and compile; the dead-end DLL plugin system is removed
**Depends on**: Nothing (first phase)
**Requirements**: STATE-01, PLUG-01, PLUG-07, SUB-01, OVER-01, SIM-02, SIM-04, SIM-05, UI-01, CAP-01
**Success Criteria** (what must be TRUE):
  1. All v2 interfaces (IApplicationState, IOverlay, IPlugin, IStateUI) and base types (AppSubsystem, StateSubsystem) compile with lifecycle method signatures
  2. ServiceProvider concept constrains types at compile time; StandaloneServiceProvider satisfies it and provides subsystem access without a window or GPU
  3. StateMachine<TStateId> processes flat transitions and push/pop correctly in unit tests
  4. Capability tag constants (Simulation, Rendering, Presentation, Editing) are declared as GameplayTag values and usable in tests
  5. DLL plugin system fully removed -- no PluginExport.h, PluginLoader.h, or CreateGamePlugin references remain
**Plans:** 4 plans

Plans:
- [x] 01-01-PLAN.md -- Tag system rename (GameplayTag -> Tag) + FromName/FromInterned removal + AddTags
- [x] 01-02-PLAN.md -- V2 vocabulary types: interfaces, concepts, base types, service provider, capabilities
- [x] 01-03-PLAN.md -- StateMachine<TStateId> generic template with descriptor validation
- [x] 01-04-PLAN.md -- DLL plugin system removal and sandbox migration to explicit main()

### Phase 2: Subsystem Infrastructure
**Goal**: SubsystemRegistry and EngineContext v2 form the proven spine of the new architecture with dependency ordering and capability activation
**Depends on**: Phase 1
**Requirements**: SUB-02, SUB-03, SUB-04, SUB-05, SUB-06, SUB-07, CAP-02, CAP-03, CAP-04, CAP-05
**Success Criteria** (what must be TRUE):
  1. SubsystemRegistry initialises subsystems in topological DependsOn order; cycles are detected at Finalise() time; shutdown runs in reverse order
  2. Subsystems with RequiredCapabilities activate only when the current capability set satisfies them; empty RequiredCapabilities means always active; the rule applies uniformly to subsystems, overlays, and render features
  3. Capability set is computed from two sources (app-level + state-level) and batched during transitions with no intermediate empty state
  4. Abstract-type resolution allows querying a subsystem by interface type; Initialise() returns Result<void> with error propagation on first failure
  5. EngineContext v2 provides typed access to app-scoped and state-scoped subsystem registries; StateSubsystems is the renamed GameSubsystems static accessor
**Plans:** 2 plans

Plans:
- [x] 02-01-PLAN.md - SubsystemRegistry with dependency ordering and capability-gated activation
- [x] 02-02-PLAN.md - EngineContext v2 facade, ComputeEffectiveCaps, and EngineContextRef ECS singleton

### Phase 3: Plugin Composition
**Goal**: Plugins compose the application through AppBuilder with typed registrar store, dependency ordering, and per-plugin configuration
**Depends on**: Phase 2
**Requirements**: PLUG-02, PLUG-03, PLUG-04, PLUG-05, PLUG-06, CFG-01, CFG-02, APP-01
**Success Criteria** (what must be TRUE):
  1. AppBuilder accepts plugin registration via AddPlugin<T>(), resolves dependencies, and calls Build() in topological order
  2. Plugin group types compose multiple plugins into a single AddPlugin call
  3. AppBuilder::Finalise() produces an immutable AppDescriptor snapshot; validation catches undeclared capabilities, missing dependencies, and duplicate registrations
  4. Plugins register lifecycle hooks (OnAppReady, OnStateEnter, etc.) that fire at the correct lifecycle points
  5. Per-plugin configuration loads from TOML sections via AppBuilder::LoadConfig<T>(section) with caching
**Plans:** 5 plans

Plans:
- [x] 03-01-PLAN.md -- SubsystemManifest retrofit + TopologicalSort extraction
- [x] 03-02-PLAN.md -- Plugin foundation types (IRegistrar, PluginDescriptor, concepts, AppDescriptor)
- [x] 03-03-PLAN.md -- AppBuilder core + existing registrar adaptation + tests
- [x] 03-04-PLAN.md -- ConfigService, ConfigRegistrar, 3-tier TOML layering
- [x] 03-05-PLAN.md -- Application::AddPlugin<T>() + EngineContext AppDescriptor access

### Phase 4: Orchestration
**Goal**: ApplicationStateMachine, Simulation, and OverlayStack operate together with full lifecycle control and state transition management
**Depends on**: Phase 3
**Requirements**: STATE-02, STATE-03, STATE-04, STATE-05, STATE-08, SIM-01, SIM-03, SIM-06, SIM-07, OVER-02, OVER-03, OVER-04, UI-02, UI-03
**Success Criteria** (what must be TRUE):
  1. ApplicationStateMachine processes flat transitions (replace) with deferred execution at frame boundaries; only declared transitions are allowed; undeclared transitions fail at startup
  2. Push/pop modal stack works with suspend/resume negotiation (BackgroundPreferences + SuspensionPolicy intersection); state subsystems persist across push/pop without teardown
  3. Simulation replaces Game as the flecs world owner with ServiceProvider-based access; headless simulation works via StandaloneServiceProvider; ActiveGameState singleton updates via transition callbacks
  4. OverlayStack executes overlays in registration order (input top-down, render bottom-up) with capability-gated activation and runtime toggle (ActivateOverlay/DeactivateOverlay)
  5. IStateUI and sub-state machines can be registered per-state via builder and their lifecycle mirrors the owning state (attach/detach/suspend/resume)
**Plans:** 6 plans

Plans:
- [ ] 04-01-PLAN.md -- Orchestration vocabulary types + IOverlay::OnEvent signature change
- [ ] 04-02-PLAN.md -- ApplicationStateMachine core (flat/push/pop, deferred, graph validation)
- [ ] 04-03-PLAN.md -- OverlayStack (capability-gated, event consumption, runtime toggle)
- [ ] 04-04-PLAN.md -- Simulation StateSubsystem + EngineContextServiceProvider adapter
- [ ] 04-05-PLAN.md -- AppBuilder state/overlay/UI registration extensions + manifests
- [ ] 04-06-PLAN.md -- EngineContext Phase 4 wiring + orchestration integration tests

### Phase 5: Concrete States and Engine Decomposition
**Goal**: V2 architecture handles real workloads -- GameplayState runs simulation, EngineRuntime is split into independent AppSubsystems, rendering uses canvas submission
**Depends on**: Phase 4
**Requirements**: STATE-06, STATE-07, OVER-05, REND-01, REND-02, REND-03
**Success Criteria** (what must be TRUE):
  1. GameplayState wraps Simulation in the IApplicationState lifecycle and runs flecs world updates through the v2 frame path
  2. EditorState stub enters and exits cleanly, proving the IApplicationState pattern for future editor work
  3. FpsOverlay renders frame timing data via the OverlayStack, fully replacing FpsOverlayLayer
  4. EngineRuntime is decomposed into individual AppSubsystems (Window, Input, Time, Renderer) each with proper RAII lifecycle and dependency ordering
  5. Render submission uses typed canvas collectors (SceneCanvas, UICanvas, DebugCanvas) with capability-gated render features via SetEnabled()
**Plans**: TBD

### Phase 6: Application Rewrite and Integration
**Goal**: Application runs entirely on v2 architecture; Journey sandbox works end-to-end; all tests pass
**Depends on**: Phase 5
**Requirements**: APP-02, APP-03, APP-04
**Success Criteria** (what must be TRUE):
  1. Application::Loop() runs the v2 frame sequence (ProcessPending -> events -> state update -> render) with no v1 codepath
  2. Journey sandbox boots through AddPlugin<T>(), enters GameplayState, and renders frames
  3. All existing tests pass against the v2 architecture (updated or rewritten as needed)
**Plans**: TBD

### Phase 7: Cleanup
**Goal**: All dead v1 code removed; codebase is purely v2 with no vestigial types or includes
**Depends on**: Phase 6
**Requirements**: CLN-01, CLN-02, CLN-03, CLN-04, CLN-05
**Success Criteria** (what must be TRUE):
  1. LayerStack, Layer, and all layer-related code are removed from engine and tests
  2. Old EngineRuntime class and old EngineContext struct (27-line version) are removed
  3. Game class is removed after Simulation is proven
  4. Old Plugin base class is removed after IPlugin replaces it
  5. Codebase compiles cleanly with no dead includes, forward declarations, or references to removed types
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Foundation Types | 0/? | Not started | - |
| 2. Subsystem Infrastructure | 0/? | Not started | - |
| 3. Plugin Composition | 0/? | Not started | - |
| 4. Orchestration | 0/? | Not started | - |
| 5. Concrete States and Engine Decomposition | 0/? | Not started | - |
| 6. Application Rewrite and Integration | 0/? | Not started | - |
| 7. Cleanup | 0/? | Not started | - |
