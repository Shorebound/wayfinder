# Requirements: Wayfinder Application Architecture v2 Migration

**Defined:** 2026-04-03
**Core Value:** The engine's application architecture is clean, extensible, and compositional - so that every future feature plugs in through well-defined extension points rather than fighting a monolithic runtime.

## v1 Requirements

Requirements for this milestone. Each maps to roadmap phases.

### Application States

- [x] **STATE-01**: IApplicationState interface with full lifecycle (OnEnter, OnExit, OnSuspend, OnResume, OnUpdate, OnRender, OnEvent)
- [ ] **STATE-02**: ApplicationStateMachine with flat transitions (replace) and push/pop modal stack
- [ ] **STATE-03**: Deferred state transitions processed at frame boundaries
- [ ] **STATE-04**: State transition validation (declared transitions only, startup error on undeclared)
- [ ] **STATE-05**: Push/pop negotiation (BackgroundPreferences + SuspensionPolicy intersection)
- [ ] **STATE-06**: GameplayState wrapping Simulation into IApplicationState lifecycle
- [ ] **STATE-07**: EditorState stub proving the IApplicationState pattern
- [ ] **STATE-08**: State subsystems persist across push/pop (no teardown for modal states)

### Plugins

- [x] **PLUG-01**: IPlugin interface with Build-only pattern (no OnStartup/OnShutdown)
- [ ] **PLUG-02**: AppBuilder replacing PluginRegistry with typed registrar store
- [ ] **PLUG-03**: Plugin dependency declaration and topological ordering of Build() calls
- [ ] **PLUG-04**: Plugin group types for composing plugin sets
- [ ] **PLUG-05**: AppDescriptor as read-only snapshot from AppBuilder::Finalise()
- [ ] **PLUG-06**: Lifecycle hook registration (OnAppReady, OnStateEnter<T>, etc.)
- [x] **PLUG-07**: DLL plugin system removal (PluginExport.h, PluginLoader.h, CreateGamePlugin)

### Subsystems

- [x] **SUB-01**: AppSubsystem and StateSubsystem scoped base classes
- [ ] **SUB-02**: SubsystemRegistry with DependsOn and topological sort for init/shutdown
- [ ] **SUB-03**: Capability-based activation for state-scoped subsystems
- [ ] **SUB-04**: Abstract-type resolution (register concrete under both concrete and abstract type_index)
- [ ] **SUB-05**: SubsystemRegistry::Initialise returns Result<void> with error propagation
- [ ] **SUB-06**: EngineContext as central service-access mechanism (subsystem queries, state transitions, overlay ops)
- [ ] **SUB-07**: StateSubsystems accessor renamed from GameSubsystems, bound/unbound on state transitions

### Capabilities

- [x] **CAP-01**: Capability tags as GameplayTag values (Simulation, Rendering, Presentation, Editing)
- [ ] **CAP-02**: Two-source model (app-level + state-level capabilities)
- [ ] **CAP-03**: Uniform activation rules for subsystems, overlays, and render features
- [ ] **CAP-04**: Empty RequiredCapabilities = always active
- [ ] **CAP-05**: Capability set batched during transitions (no intermediate empty state)

### Overlays

- [x] **OVER-01**: IOverlay interface with attach/detach/update/render/event lifecycle
- [ ] **OVER-02**: OverlayStack with registration-order execution (input top-down, render bottom-up)
- [ ] **OVER-03**: Capability-gated activation/deactivation on state transitions
- [ ] **OVER-04**: Runtime toggle (ActivateOverlay/DeactivateOverlay)
- [ ] **OVER-05**: FpsOverlay rewritten from FpsOverlayLayer

### Simulation

- [ ] **SIM-01**: Simulation class replacing Game (flecs world + scene management only)
- [x] **SIM-02**: ServiceProvider concept for dependency injection
- [ ] **SIM-03**: EngineContextServiceProvider adapter for live usage
- [x] **SIM-04**: StandaloneServiceProvider for headless tests and tools
- [x] **SIM-05**: StateMachine<TStateId> generic template for per-state sub-state machines
- [ ] **SIM-06**: Sub-state registration via builder.ForState<T>().RegisterSubState()
- [ ] **SIM-07**: ActiveGameState singleton updated via transition callbacks

### State UI

- [x] **UI-01**: IStateUI interface for plugin-injected per-state UI
- [ ] **UI-02**: Registration via builder.ForState<T>().RegisterStateUI<U>()
- [ ] **UI-03**: Lifecycle mirroring state (attach/detach/suspend/resume/update/render/event)

### Configuration

- [ ] **CFG-01**: Per-plugin configuration replacing monolithic EngineConfig
- [ ] **CFG-02**: AppBuilder::LoadConfig<T>(section) with cached loading

### Rendering Integration

- [ ] **REND-01**: EngineRuntime decomposed into individual AppSubsystems (Window, Input, Time, Renderer)
- [ ] **REND-02**: Canvas-based render submission model (typed per-frame data collectors)
- [ ] **REND-03**: Render features with capability-gated activation via SetEnabled()

### Application

- [ ] **APP-01**: Application::AddPlugin<T>() as sole public API for composition
- [ ] **APP-02**: v2 main loop (ProcessPending -> events -> update -> render)
- [ ] **APP-03**: Journey sandbox updated to use new architecture
- [ ] **APP-04**: Existing tests updated or rewritten for new architecture

### Cleanup

- [ ] **CLN-01**: LayerStack and Layer removal
- [ ] **CLN-02**: Old EngineRuntime removal
- [ ] **CLN-03**: Old EngineContext (27-line struct) removal
- [ ] **CLN-04**: Game class removal after Simulation proves out
- [ ] **CLN-05**: Old Plugin base class removal after IPlugin replaces it

## v2 Requirements

Deferred to future milestones. Tracked but not in current roadmap.

### Editor

- **EDIT-01**: Full editor implementation with docking, panels, viewport
- **EDIT-02**: Visual UI designer tooling (UMG-style)
- **EDIT-03**: Undo/redo command pattern

### Extensions

- **EXT-01**: Hot-reload of plugins or subsystems
- **EXT-02**: Config hot-reload via file watcher
- **EXT-03**: Scripting integration as subsystem/plugin
- **EXT-04**: Tag-gated widget activation for UI toolkit

### Performance

- **PERF-01**: Multi-threaded render extraction
- **PERF-02**: Async asset loading with engine awaitables
- **PERF-03**: Flecs built-in multithreading pipeline

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| DLL/shared library plugin loading | v2 removes this; static linking, game owns main() |
| Automatic plugin discovery (scanning) | Complexity without benefit for compiled C++ engine |
| Runtime capability modification mid-state | Creates complex activation cascades; reconfigure by transitioning states |
| Hierarchical finite state machines (HFSM) | Flat + push/pop + per-state sub-states covers all identified cases |
| Global state/singleton access pattern (new) | EngineContext by reference; StateSubsystems bounded accessor for flecs constraint |
| Asset pipeline overhaul | Migration-forced changes only; file issues for larger work |
| Networking, audio subsystem implementation | Future plugins using the new architecture |
| UI toolkit / widget system internals | IStateUI interface only, no concrete toolkit |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| STATE-01 | Phase 1 | Complete |
| STATE-02 | Phase 4 | Pending |
| STATE-03 | Phase 4 | Pending |
| STATE-04 | Phase 4 | Pending |
| STATE-05 | Phase 4 | Pending |
| STATE-06 | Phase 5 | Pending |
| STATE-07 | Phase 5 | Pending |
| STATE-08 | Phase 4 | Pending |
| PLUG-01 | Phase 1 | Complete |
| PLUG-02 | Phase 3 | Pending |
| PLUG-03 | Phase 3 | Pending |
| PLUG-04 | Phase 3 | Pending |
| PLUG-05 | Phase 3 | Pending |
| PLUG-06 | Phase 3 | Pending |
| PLUG-07 | Phase 1 | Complete |
| SUB-01 | Phase 1 | Complete |
| SUB-02 | Phase 2 | Pending |
| SUB-03 | Phase 2 | Pending |
| SUB-04 | Phase 2 | Pending |
| SUB-05 | Phase 2 | Pending |
| SUB-06 | Phase 2 | Pending |
| SUB-07 | Phase 2 | Pending |
| CAP-01 | Phase 1 | Complete |
| CAP-02 | Phase 2 | Pending |
| CAP-03 | Phase 2 | Pending |
| CAP-04 | Phase 2 | Pending |
| CAP-05 | Phase 2 | Pending |
| OVER-01 | Phase 1 | Complete |
| OVER-02 | Phase 4 | Pending |
| OVER-03 | Phase 4 | Pending |
| OVER-04 | Phase 4 | Pending |
| OVER-05 | Phase 5 | Pending |
| SIM-01 | Phase 4 | Pending |
| SIM-02 | Phase 1 | Complete |
| SIM-03 | Phase 4 | Pending |
| SIM-04 | Phase 1 | Complete |
| SIM-05 | Phase 1 | Complete |
| SIM-06 | Phase 4 | Pending |
| SIM-07 | Phase 4 | Pending |
| UI-01 | Phase 1 | Complete |
| UI-02 | Phase 4 | Pending |
| UI-03 | Phase 4 | Pending |
| CFG-01 | Phase 3 | Pending |
| CFG-02 | Phase 3 | Pending |
| REND-01 | Phase 5 | Pending |
| REND-02 | Phase 5 | Pending |
| REND-03 | Phase 5 | Pending |
| APP-01 | Phase 3 | Pending |
| APP-02 | Phase 6 | Pending |
| APP-03 | Phase 6 | Pending |
| APP-04 | Phase 6 | Pending |
| CLN-01 | Phase 7 | Pending |
| CLN-02 | Phase 7 | Pending |
| CLN-03 | Phase 7 | Pending |
| CLN-04 | Phase 7 | Pending |
| CLN-05 | Phase 7 | Pending |

**Coverage:** 56/56 v1 requirements mapped (100%)
- v1 requirements: 47 total
- Mapped to phases: 0
- Unmapped: 47

---
*Requirements defined: 2026-04-03*
*Last updated: 2026-04-03 after initialization*
