# Feature Landscape: Game Engine Application Architecture

**Domain:** C++ game engine application framework (app shell, plugin composition, state management, subsystem lifetime)
**Researched:** 2026-04-03
**Overall confidence:** HIGH (based on analysis of Unreal, Bevy, Godot, O3DE, Fyrox, and existing v2 plans)

---

## Table Stakes

Features users expect from a modern engine application framework. Missing any of these makes the architecture feel incomplete or forces workarounds.

### 1. Application State Machine

| Feature | Why Expected | Complexity | Engine Precedents | Notes |
|---------|--------------|------------|-------------------|-------|
| Named application states with enter/exit lifecycle | Every engine has this; it's how you separate splash/menu/gameplay/editor | Low | Unreal GameMode, Bevy States, Godot SceneTree root swap | Wayfinder v2 has this via `IApplicationState` |
| State transition validation | Prevents impossible transitions, catches typos at startup | Low | Bevy validates state transitions, Unreal validates GameMode | v2 has `AddTransition<A, B>()` -- good |
| Deferred transitions (queued, processed at frame boundary) | Prevents mid-frame teardown/corruption | Low | Universal -- Unreal defers world travel, Bevy defers state changes, Godot defers `queue_free()` | v2 does this correctly |
| Suspend/Resume for modal push/pop | Pause menus, settings screens, confirmation dialogs | Med | Unreal uses UMG widget stack; Bevy pushes sub-states; LibGDX Screen stack | v2's hybrid flat+push/pop is clean |
| Initial state declaration | One state must be first; validated at startup | Low | Bevy requires initial state, Unreal has default GameMode | v2 has `.Initial = true` validation |

### 2. Plugin/Extension Composition

| Feature | Why Expected | Complexity | Engine Precedents | Notes |
|---------|--------------|------------|-------------------|-------|
| Plugin as sole extension unit | Clean composition -- everything registered through one mechanism | Low | Bevy Plugin, O3DE Gem/Module, Fyrox Plugin | v2's `IPlugin::Build(AppBuilder&)` |
| Plugin dependency declaration and ordering | Plugins must init in dependency order; missing deps caught at startup | Med | Bevy checks `is_plugin_added()`, O3DE Gem dependencies, Unreal module dependencies | v2 has `builder.DependsOn<CorePlugin>()` |
| Plugin groups / bundles | Compose sets of plugins into logical units (DefaultPlugins, MinimalPlugins) | Low | Bevy `PluginGroup`/`DefaultPlugins`, O3DE Gem bundles | v2 has plugin group types (IPlugin composites) |
| Build-only plugin interface (no lifecycle methods) | Plugins register interest, they don't manage their own lifetime | Low | Bevy `Plugin::build()` is the primary method; `finish()`/`cleanup()` are secondary | v2 correctly removes `OnStartup`/`OnShutdown` |
| Lifecycle hook registration | Plugins register callbacks for specific lifecycle events (app ready, state enter/exit) | Med | Bevy observers/events, Unreal delegates, O3DE notification bus | v2 has `OnAppReady()`, `OnStateEnter<T>()` |

### 3. Subsystem/Service Lifetime Management

| Feature | Why Expected | Complexity | Engine Precedents | Notes |
|---------|--------------|------------|-------------------|-------|
| Scoped subsystem lifetimes (app vs state) | Services should only exist when relevant; physics doesn't need to live during splash screen | Med | Unreal USubsystem hierarchy (UGameInstanceSubsystem, UWorldSubsystem, ULocalPlayerSubsystem), Bevy Resources exist app-wide but systems are schedule-gated | v2's `AppSubsystem`/`StateSubsystem` split |
| Dependency-ordered init/shutdown | Init follows dependency graph; shutdown is reverse order | Med | Universal in mature engines; O3DE Gem activation order, Unreal Module loading order | v2 has `DependsOn` with topological sort |
| Abstract-type resolution | Access subsystem by interface, not concrete type; allows backend swapping | Med | Unreal `GetSubsystem<T>()` with UCLASS hierarchy, O3DE `AZ::Interface<T>` | v2 has `RegisterSubsystem<SDL3Window, Window>()` |
| Subsystem init failure propagation | If a subsystem fails to init, the state transition is aborted cleanly | Med | O3DE `Activate()` can fail, Unreal logs errors but continues | v2 returns `Result<void>` from `Initialise()` |
| Central service locator (non-global) | States/overlays/systems access services without globals or singletons | Med | Bevy World as service locator, Unreal UWorld subsystems, O3DE `AZ::Interface<T>` | v2's `EngineContext` |

### 4. Overlay/Decorator System

| Feature | Why Expected | Complexity | Engine Precedents | Notes |
|---------|--------------|------------|-------------------|-------|
| Debug overlay (FPS, frame time, entity count) | Every engine ships one; first thing developers look for | Low | Unreal `stat fps`, Bevy `FrameTimeDiagnosticsPlugin`, Godot built-in debug info | v2's `FpsOverlay` |
| Overlay as separate concept from state | Debug overlays persist across state transitions; they're decorations, not phases | Med | Unreal Slate debug layers, Bevy Gizmo plugins | v2 correctly separates overlays from states |
| Input consumption by overlays | Debug console must consume keyboard input when active, not pass to game | Low | Universal -- every overlay system handles this | v2 has top-down event dispatch with consumption |
| Runtime toggle (activate/deactivate) | Toggle debug console with tilde, profiler with F3, etc. | Low | Universal | v2 has `ActivateOverlay()`/`DeactivateOverlay()` |

### 5. Configuration Management

| Feature | Why Expected | Complexity | Engine Precedents | Notes |
|---------|--------------|------------|-------------------|-------|
| Per-plugin/subsystem config instead of monolithic | Each subsystem owns its config type; only loaded if the plugin is present | Med | Unreal `.ini` per module, Bevy Resources, O3DE per-Gem settings | v2's `builder.LoadConfig<PhysicsConfig>("physics")` |
| File-based config (TOML/JSON/INI) | Human-editable, version-controllable | Low | Unreal INI, Godot `project.godot`, Bevy uses Resources but TOML/RON common | Wayfinder uses TOML -- good |
| Config validation at load time | Clear error messages for malformed config; don't silently use defaults | Low | Unreal validates INI sections, O3DE validates settings schema | v2 notes this but needs explicit implementation |

### 6. Headless/Tool Usage

| Feature | Why Expected | Complexity | Engine Precedents | Notes |
|---------|--------------|------------|-------------------|-------|
| Simulation usable without window/GPU | Tests, CLI tools, server builds must work headless | Med | Unreal `-NullRHI`, Bevy `MinimalPlugins`/`HeadlessRenderPlugin`, Godot `--headless` | v2's `Simulation` + `StandaloneServiceProvider` |
| Backend selection via plugin composition | Swap GPU/Null/headless at composition time, not #ifdef | Med | Bevy plugin swap (`WinitPlugin` vs `ScheduleRunnerPlugin`), O3DE null renderer module | v2 has `NullRendererPlugin` vs `VulkanRendererPlugin` |
| App::update() single-frame stepping | Tests can advance one frame at a time | Low | Bevy `app.update()`, Godot `_process()` testable via SceneTree | v2 doesn't expose this explicitly -- needed |
| Engine-as-library (game owns main) | Embedding, testing, tooling all require library usage | Low | Bevy `App::new().run()`, Fyrox `Plugin`, O3DE launcher | Wayfinder v2 `Application` is a type, game owns `main()` |

---

## Differentiators

Features that set the architecture apart. Not expected by default, but valued when present. These represent competitive advantage over typical engine frameworks.

### 1. Capability-Gated Activation System

| Feature | Value Proposition | Complexity | Engine Precedents | Notes |
|---------|-------------------|------------|-------------------|-------|
| `GameplayTag`-based capability activation for subsystems, overlays, render features | Single mechanism controls what's active. Adding a new state auto-activates the right subsystems/overlays/features without touching other plugins | Med | Unreal Lyra uses `GameplayTags` for HUD activation, GAS ability tags. No engine unifies subsystem+overlay+render activation through one tag system | v2's strongest differentiator. Bevy's `run_if()` is per-system; Unreal's subsystem activation is world-lifetime. Neither has this unified approach |
| Two-source capability model (app-level + state-level) | Hardware capabilities (GPU present) and state capabilities (Simulation active) compose naturally | Med | Unique. Unreal has hardware feature levels but doesn't compose them with gameplay state | Clean solution to "this overlay only makes sense during gameplay" |
| Empty capabilities = always active | Sensible default -- FPS overlay, present pass, etc. need no special config | Low | Novel default -- most engines require explicit enable | Reduces boilerplate |

### 2. Push/Pop Negotiation Protocol

| Feature | Value Proposition | Complexity | Engine Precedents | Notes |
|---------|-------------------|------------|-------------------|-------|
| Background render/update negotiation between pushed and suspended states | Pause menu can render game behind it (frosted glass) but not update physics. Both sides agree | Med | No engine does this cleanly. Unreal uses widget stacking. Godot pauses the whole tree or doesn't. Bevy has no modal state concept | `BackgroundPreferences` + `SuspensionPolicy` intersection is elegant |
| State subsystems persist across push/pop | PauseState can still access PhysicsSubsystem (to read, not tick) without re-init | Low | Novel -- Unreal subsystems are world-scoped, Bevy resources are app-scoped | Avoids expensive teardown for modal states |

### 3. Plugin-Injected Per-State UI (IStateUI)

| Feature | Value Proposition | Complexity | Engine Precedents | Notes |
|---------|-------------------|------------|-------------------|-------|
| Separate game UI injection per app state | Multiple plugins contribute UI to the same state. State is reusable (GameplayState doesn't know about health bars) | Med | Unreal Lyra uses `UCommonActivatableWidget` with gameplay tags. Bevy has no built-in per-state UI abstraction. Godot bakes UI into scenes | v2's `IStateUI` cleanly separates state logic from state presentation |
| IStateUI lifecycle mirrors state lifecycle (suspend/resume) | Push PauseState -> game HUD grays out; pop -> HUD resumes animations | Low | Lyra does something similar; most engines don't | Small but satisfying detail |

### 4. Typed Registrar Store (AppBuilder)

| Feature | Value Proposition | Complexity | Engine Precedents | Notes |
|---------|-------------------|------------|-------------------|-------|
| Domain-specific registrars with validation (not a flat registration bag) | SystemRegistrar does topological sort, StateRegistrar validates one initial state, TagRegistrar merges files. Each domain's logic is encapsulated | Med | O3DE has typed component descriptors but not a general registrar pattern. Bevy's `App` is a flat bag of operations | Extensible -- ReplicationRegistrar, AudioRegistrar etc. can be added by plugins without modifying AppBuilder |
| Custom registrar types from game code | `builder.Registrar<ReplicationRegistrar>()` -- extend the builder without engine changes | Med | Novel. Bevy's approach requires App trait extensions. Unreal uses UCLASS reflection | Future-proof for networking, modding, etc. |

### 5. ServiceProvider Concept (Dependency Inversion)

| Feature | Value Proposition | Complexity | Engine Precedents | Notes |
|---------|-------------------|------------|-------------------|-------|
| Simulation parameterised on ServiceProvider concept | Same Simulation code runs in GameplayState (EngineContextServiceProvider), CLI tool (StandaloneServiceProvider), tests (mock provider) | Med | Bevy's World IS the service provider (tight coupling). Unreal subsystems are world-bound. No engine cleanly separates simulation from its service source | Enables true headless testing without engine overhead |
| StandaloneServiceProvider for tools/tests | Registry-based, type-erased. No engine context needed | Low | Novel as a first-class engine feature | Massive testing ergonomics |

### 6. Sub-State Machines Owned by Application States

| Feature | Value Proposition | Complexity | Engine Precedents | Notes |
|---------|-------------------|------------|-------------------|-------|
| Generic `StateMachine<TStateId>` as member variable, not engine-managed | GameplayState owns Playing/Loading/Cutscene. EditorState owns Editing/PlayMode. No shared infrastructure bloat | Med | Bevy has `SubStates` and `ComputedStates` (enum-based, global). Unreal's game state machine is custom per-game | Cleaner than Bevy's global state approach for per-state internal flow |
| Plugin-declared sub-states with transition validation | `ForState<GameplayState>().RegisterSubState({.Name = "Playing"})` | Med | Novel -- Bevy's sub-states are compile-time enum variants, not plugin-declared | Extensibility without modifying the state type |

---

## Anti-Features

Features to explicitly NOT build at this stage. Each has a rationale for exclusion.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| **DLL/shared library plugin loading** | No runtime mod ecosystem, increases complexity, ABI fragility, v2 explicitly removes `PluginLoader`/`PluginExport.h` | Static linking. Game owns `main()`. `@todo` note for future hot-reload if needed |
| **Hot-reload of plugins or subsystems** | Extremely complex in C++ (ABI, state preservation, resource invalidation), premature without a user base | Architecture is compatible with future hot-reload (config callback stubs). Build that bridge when you get there |
| **Hierarchical state machines (HFSM)** | Adds complexity; flat + push/pop + per-state sub-state machines cover all identified use cases | `StateMachine<InternedString>` per-state for internal flow. Hierarchy via push/pop for modal overlaying |
| **Global state/singleton access pattern** | Defeats testability, creates hidden coupling, makes headless usage painful | `EngineContext` passed by reference. `ServiceProvider` concept for simulation. `StateSubsystems::Get<T>()` is bounded (set/unset on state transitions) |
| **Full editor implementation** | Massive scope, not needed for the migration milestone, would delay the core architecture | `EditorState` stub proving the pattern. Real editor is a future milestone |
| **UI toolkit / widget system** | Orthogonal to application architecture; IStateUI interface is the extension point | Define `IStateUI` interface only. Widget system is a separate subsystem/plugin |
| **Multi-threaded frame loop** | Architecture should BE thread-ready but implementing parallelism before correctness is premature | Design with isolation in mind (no shared mutable state in interfaces). Add threading incrementally later |
| **Automatic plugin discovery (scanning)** | Complexity without clear benefit for a compiled C++ engine. Increases startup time, reduces determinism | Explicit `AddPlugin<T>()`. Plugin groups for convenience |
| **Runtime capability modification** | Capabilities changing mid-state creates complex activation/deactivation cascades | Capabilities are fixed per app-level (startup) and per state-level (state provides). Reconfigure by transitioning states |
| **Undo/redo system** | Editor concern, not application architecture | Future editor milestone. Command pattern in `EditorState`'s domain |
| **Scripting integration** | Orthogonal to app architecture; would be a subsystem/plugin | Future milestone. Designed to plug into the existing subsystem/plugin architecture |
| **Config hot-reload** | v2 notes this as `@todo` with callback stubs. Not needed for migration | Stub `OnConfigReloaded()` callback interface. Implement file watcher later |

---

## Feature Dependencies

```
Plugin System ──────────────> AppBuilder (typed registrars)
                                   │
                    ┌──────────────┼──────────────┐
                    ▼              ▼              ▼
           State Registration  Subsystem     Overlay
                    │          Registration  Registration
                    ▼              │              │
           ApplicationState   SubsystemRegistry  OverlayStack
           Machine                 │
                    │              ▼
                    ├──> EngineContext (central access)
                    │         │
                    │         ▼
                    │    Capability System (activation gating)
                    │         │
                    ▼         ▼
             IApplicationState lifecycle
                    │
          ┌─────────┼──────────┐
          ▼         ▼          ▼
    GameplayState  EditorState  SplashState
          │         │
          ▼         ▼
    Simulation (ServiceProvider concept)
          │
    ┌─────┼──────┐
    ▼     ▼      ▼
  StateMachine  IStateUI  Scene
  (sub-states)
```

**Critical path (must be built in order):**
1. EngineContext + ServiceProvider concept (foundation for everything)
2. AppBuilder + IPlugin (composition mechanism)
3. SubsystemRegistry with scoping (AppSubsystem/StateSubsystem)
4. Capability system (activation gating)
5. ApplicationStateMachine (IApplicationState lifecycle)
6. OverlayStack (IOverlay lifecycle)
7. Simulation (renamed Game, uses ServiceProvider)
8. GameplayState + EditorState (concrete IApplicationState implementations)
9. StateMachine<T> + IStateUI (per-state internals)

---

## MVP Recommendation

**Prioritise (required for the architecture to function):**

1. **EngineContext as central service locator** - everything depends on this
2. **AppBuilder + IPlugin with dependency-ordered Build()** - composition mechanism
3. **SubsystemRegistry with AppSubsystem/StateSubsystem scoping** - services need lifetime management
4. **Capability system (GameplayTag-based activation)** - v2's core innovation; build it early or risk retrofitting
5. **ApplicationStateMachine with IApplicationState lifecycle** - the state machine IS the app loop
6. **OverlayStack with IOverlay** - debug overlay needed immediately for development
7. **Simulation (renamed Game) with ServiceProvider** - simulation must be headless-capable
8. **GameplayState** - first real proof that the whole system works end-to-end
9. **FpsOverlay** - first overlay, proves the overlay system

**Defer (valuable but not blocking):**

- **EditorState stub**: proves the pattern but can come after GameplayState works
- **IStateUI**: interface only, no concrete toolkit
- **StateMachine<T> for sub-states**: GameplayState can start with hardcoded initial state
- **Per-plugin config**: can migrate incrementally; monolithic EngineConfig works during transition
- **Push/pop negotiation**: flat transitions handle the MVP; push/pop adds modal support later
- **Custom registrar extensibility**: typed registrars work; custom plugin-defined registrars can wait

---

## Engine Comparison Matrix

| Feature | Wayfinder v2 | Unreal | Bevy | Godot | O3DE | Fyrox |
|---------|-------------|--------|------|-------|------|-------|
| **State machine** | Hybrid flat+push/pop, typed C++ states | GameMode per level, custom FSMs | Global enum States + SubStates + ComputedStates | SceneTree root swap | No built-in app state machine | No built-in |
| **Plugin system** | `IPlugin::Build(AppBuilder&)`, dependency-ordered | UE Modules + Plugins (DLL), UE5 Gameplay Features | `Plugin::build(&mut App)`, PluginGroup | EditorPlugin (editor only), GDExtension | Gem system (CMake modules), AZ::Module | Plugin trait (game entry point) |
| **Subsystem scoping** | App-scoped + State-scoped, capability-gated | UGameInstanceSubsystem + UWorldSubsystem + ULocalPlayerSubsystem | App-wide Resources (no scoping) | Autoloads (global), Nodes (scene-scoped) | SystemComponent (module-scoped) | No formal scoping |
| **Service access** | EngineContext (passed by ref, no globals) | UWorld::GetSubsystem, UGameInstance singletons | World as implicit service locator (ECS queries) | Autoloads (global singletons) | AZ::Interface<T> (global) | Plugin struct members |
| **Capability gating** | GameplayTag-based unified activation | Lyra GameplayTags (per-feature), GAS ability tags | `run_if()` per-system/schedule | Node process mode (inherit/pausable/always) | Feature flags per Gem | None |
| **Overlay system** | IOverlay + OverlayStack, capability-gated | Slate debug layers, stat commands, custom HUDs | Gizmo plugins, custom overlay systems | CanvasLayer nodes in SceneTree | ImGui integration, debug drawing | Built-in debug draw |
| **Config model** | Per-plugin TOML files, cached loader | Per-module .ini files, console variables | Resources (code), RON/TOML (community) | project.godot + .tres resources | Settings registry (JSON) | No formal config |
| **Headless support** | StandaloneServiceProvider, NullRenderer plugin | -NullRHI flag, dedicated server builds | MinimalPlugins, HeadlessRenderPlugin | --headless flag | Null renderer module | Limited |
| **Editor integration** | EditorState (same architecture as game) | Separate editor process, PIE shares GameMode | Separate app (bevy_editor_pls community) | Integrated editor (GDScript/C#) | Integrated editor (Qt) | fyrox-editor (separate binary) |
| **Thread readiness** | Interfaces designed for future parallelism | Fully multithreaded (game thread + render thread + workers) | Automatic multithreaded system scheduling | Single-threaded (workers for loading) | Multi-threaded job system | Single-threaded with task pool |

---

## Sources

- Wayfinder v2 design documents: `docs/plans/application_architecture_v2.md`, `docs/plans/game_framework.md`, `docs/plans/application_migration_v2.md`
- Bevy 0.18 documentation: `bevy_app::App` API, Plugin trait, States system (docs.rs/bevy - HIGH confidence)
- Unreal Engine architecture: GameMode/GameState lifecycle, USubsystem hierarchy, Lyra sample project patterns (training data - MEDIUM confidence, well-known stable architecture)
- Godot 4 documentation: SceneTree, Node lifecycle, EditorPlugin, MainLoop (training data - MEDIUM confidence)
- O3DE Gem system: AZ::Module, SystemComponent, AZ::Interface<T> service locator (training data - MEDIUM confidence)
- Fyrox Plugin trait: game entry point pattern (training data - MEDIUM confidence)
