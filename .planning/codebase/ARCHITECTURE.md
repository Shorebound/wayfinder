# Architecture

**Last mapped:** 2026-04-03
**Focus areas:** core, app, plugins, gameplay/simulation

---

## High-Level Pattern

Wayfinder is a **modular, plugin-composed game engine** with a data-driven ECS core. The engine is a library - the game (or editor) owns `main()` and constructs the `Application` type.

**Current architecture:** Monolithic `EngineRuntime` + Layer-based app lifecycle + Plugin/Registrar system + Gameplay (`Game` class with ECS world).

**Planned v2 architecture:** Plugin-composed `Application` with `ApplicationStateMachine`, capability-gated subsystems, overlay stack, and `Simulation` as a framework-agnostic ECS host. See `docs/plans/application_architecture_v2.md`.

---

## Current Architecture (v1)

### Initialisation Flow

```
main() [EntryPoint.h]
  -> CreateGamePlugin()              // User-defined factory
  -> Application(plugin, args)
  -> Application::Run()
    -> Application::Initialise()
      -> ProjectDescriptor::LoadFromFile()
      -> EngineConfig::LoadFromFile()
      -> PluginRegistry(project, config)
        -> gamePlugin.Build(registry)    // Registers systems, components, states, tags
      -> EngineRuntime::Initialise()     // Creates Window, Input, Time, RenderDevice, Renderer
      -> Game::Initialise(GameContext)   // Creates flecs world, subsystems, loads boot scene
    -> Application::Loop()
    -> Application::Shutdown()
```

### Per-Frame Loop

```
Application::Loop()
  -> SDL event polling -> EventQueue::Push()
  -> EventQueue::Drain(LayerStack::OnEvent)   // Layers handle events top-down
  -> LayerStack::OnUpdate(dt)                  // Layers update
  -> Game::Update(dt)                          // Game state machine + flecs progress
    -> GameStateMachine::Update()              // Re-evaluate run conditions
    -> flecs_world.progress(dt)                // Run active ECS systems
  -> EngineRuntime::BeginFrame()
  -> Renderer::RenderScene()                   // Render graph build + execute
  -> EngineRuntime::EndFrame()
```

### Ownership Hierarchy

```
Application
  +-- PluginRegistry              (descriptor store)
  |     +-- SystemRegistrar       (ECS system declarations)
  |     +-- StateRegistrar        (game state declarations)
  |     +-- TagRegistrar          (gameplay tag declarations)
  +-- EngineRuntime               (platform + rendering services)
  |     +-- Window                (SDL3 or Null)
  |     +-- Input                 (SDL3 or Null)
  |     +-- Time                  (SDL3 or Null)
  |     +-- RenderDevice          (SDL_GPU or Null)
  |     +-- Renderer              (render graph, features, passes)
  |     +-- SceneRenderExtractor  (ECS -> render data)
  |     +-- BlendableEffectRegistry
  +-- Game                        (simulation root)
  |     +-- flecs::world          (ECS)
  |     +-- SubsystemCollection<GameSubsystem>
  |     |     +-- GameplayTagRegistry
  |     |     +-- GameStateMachine
  |     |     +-- PhysicsSubsystem (if enabled)
  |     +-- Scene                 (current scene)
  |     +-- AssetService*         (non-owning)
  +-- LayerStack
  |     +-- [FpsOverlayLayer]     (debug, optional)
  +-- EventQueue
```

---

## Planned Architecture (v2)

Per `docs/plans/application_architecture_v2.md`, `docs/plans/application_migration_v2.md`, `docs/plans/game_framework.md`:

### Target Ownership Hierarchy

```
Application
  +-- EngineContext
  |     +-- AppSubsystemRegistry        (application lifetime)
  |     |     +-- Renderer, Window, Input, Time, AssetService, TagRegistry, ...
  |     +-- StateSubsystemRegistry      (current state lifetime)
  |           +-- PhysicsSubsystem, AudioSubsystem, ...
  +-- Plugins                           (owned permanently, Build(AppBuilder&) called once)
  +-- ApplicationStateMachine
  |     +-- [SplashState]               (provides: Presentation)
  |     +-- [GameplayState]             (provides: Simulation, Rendering, Presentation)
  |     +-- [EditorState]               (provides: Simulation, Rendering, Editing)
  +-- OverlayStack
  |     +-- [FpsOverlay]                (persistent, always active)
  |     +-- [DebugConsoleOverlay]       (persistent, toggled at runtime)
  +-- EventQueue
  +-- ProjectDescriptor
```

### Key v2 Concepts Not Yet Implemented

| Concept | Purpose | Status |
|---------|---------|--------|
| `IApplicationState` | State interface with full lifecycle (enter/exit/suspend/resume) | Not started |
| `ApplicationStateMachine` | Hybrid flat + push/pop state stack | Not started |
| `IOverlay` / `OverlayStack` | Persistent decorations with capability gating | Not started |
| Capability system | `GameplayTag`-based activation of subsystems/overlays/features | Not started |
| `AppBuilder` | Rich builder API replacing `PluginRegistry` | Not started |
| `AppSubsystem` / `StateSubsystem` | Scoped service base classes with dependency ordering | Not started (only `GameSubsystem` exists) |
| `EngineContext` (v2) | Central service-access with transition requests | Exists as minimal struct |
| `Simulation` | Renamed `Game`, framework-agnostic via `ServiceProvider` | Not started |
| `IStateUI` | Plugin-injected per-state UI provider | Not started |
| `ServiceProvider` concept | Type-erased service lookup for `Simulation` | Not started |
| Sub-state machines | `StateMachine<InternedString>` owned by `IApplicationState` | Not started |

---

## Domain Architecture

### Core (`engine/wayfinder/src/core/`)

Foundation primitives used by all other domains. No dependencies on engine subsystems.

**Key types:**
- `Result<T, E>` - error handling (alias over `std::expected`)
- `Handle<TTag>` - generational handles (20-bit index + 12-bit generation)
- `InternedString` - O(1) equality string interning
- `GameplayTag` - hierarchical dot-separated identifiers on `InternedString`
- `EventQueue` - typed, per-event-type double-buffered queue
- `ResourcePool<TTag, TResource>` - handle-indexed pool with free-list
- `StringHash` / `Uuid` / `SceneObjectId` - identity types
- `Log` - category-based logging with compile-time format validation

### App (`engine/wayfinder/src/app/`)

Application lifecycle orchestration. Owns platform services and frame loop.

**Key types:**
- `Application` - entry point, owns PluginRegistry + EngineRuntime + Game + LayerStack
- `EngineRuntime` - monolithic service container (Window, Input, Time, RenderDevice, Renderer)
- `EngineConfig` - TOML-loaded configuration
- `EngineContext` - non-owning struct: `{Window&, Input&, Time&, Config&, Project&}`
- `Layer` / `LayerStack` - update/event/render lifecycle hooks
- `Subsystem` / `SubsystemCollection<TBase>` - scoped service management
- `FpsOverlayLayer` - debug FPS display

**v2 changes:** `EngineRuntime` decomposes into individual `AppSubsystem` instances. `LayerStack` replaced by `ApplicationStateMachine` + `OverlayStack`. `EngineContext` becomes the central service-access point with subsystem queries and transition requests.

### Plugins (`engine/wayfinder/src/plugins/`)

Extension architecture. Plugins declare systems, components, states, tags during `Build()`.

**Key types:**
- `Plugin` - base class: `Build(PluginRegistry&)`, `OnStartup()`, `OnShutdown()`
- `PluginRegistry` - descriptor store with typed registrars
- `SystemRegistrar` - ECS system declarations with topological ordering
- `StateRegistrar` - game state declarations with initial state validation
- `TagRegistrar` - gameplay tag declarations (code + TOML file)
- `PluginLoader` / `PluginExport.h` - DLL plugin loading (marked for removal in v2)

**v2 changes:** `Plugin` becomes `IPlugin` with `Build(AppBuilder&)`. `PluginRegistry` replaced by `AppBuilder` (typed registrar store). `PluginLoader`/`PluginExport.h` removed (game owns `main()`). New registrations: `AddState<T>()`, `RegisterOverlay<T>()`, `RegisterRenderFeature<T>()`, `ProvideCapability()`, `ForState<T>()`.

### Gameplay (`engine/wayfinder/src/gameplay/`) -> Simulation

Game simulation root. Owns ECS world, scene management, state machine.

**Key types:**
- `Game` - simulation root: flecs world, subsystems, scene, state transitions
- `GameStateMachine` - `GameSubsystem`; manages `ActiveGameState` singleton + run conditions
- `GameState.h` - `ActiveGameState` world singleton, `RunCondition` type + helpers (`InState`, `NotInState`, `HasTag`)
- `GameplayTag` - hierarchical tags (in `core/` but heavily used here)
- `GameplayTagRegistry` - `GameSubsystem`; tag validation and file loading
- `GameContext` - lightweight init context: `{project, pluginRegistry}`

**v2 changes:** `Game` renamed to `Simulation`, stripped to flecs world + scene management. No state machine, no subsystem ownership. Services via `ServiceProvider` concept. `GameSubsystem` renamed to `StateSubsystem`. `GameStateMachine` becomes a `StateMachine<InternedString>` member variable of `IApplicationState`. `GameplayTagRegistry` moves to app-scoped subsystem.

### Scene (`engine/wayfinder/src/scene/`)

Entity/component/scene serialisation and world bootstrap.

**Key types:**
- `Scene` - manages entities in a flecs world; load/save from JSON
- `Entity` - wrapper around `flecs::entity` with typed component access
- `SceneDocument` - serialisation format for scenes (JSON, version 1)
- `RuntimeComponentRegistry` - unified engine + game component registry
- `SceneComponentRegistry` - compile-time registry of core components
- `SceneWorldBootstrap` - headless world setup for tests/tools

---

## Data Flow Patterns

### Data Flows Down

```
ProjectDescriptor -> EngineConfig -> PluginRegistry -> Game -> Scene -> Entity
                                                    -> ECS World -> Systems
```

### Events Flow Up

```
SDL Input -> EventQueue -> LayerStack -> Game (via Application loop)
                                      -> Layers consume or pass through
```

### Render Data Flow

```
ECS World -> SceneRenderExtractor -> RenderFrame (per-frame snapshot)
  -> RenderOrchestrator::BuildGraph() -> RenderGraph -> GPU execution
```

### Plugin Registration Flow

```
Plugin::Build(registry)
  -> SystemRegistrar::Register()     // ECS system declarations
  -> StateRegistrar::Register()      // Game state declarations
  -> TagRegistrar::Register()        // Tag declarations
  -> PluginRegistry stores descriptors

After all plugins:
  -> SystemRegistrar::ApplyToWorld() // Topological sort + register into flecs
  -> RuntimeComponentRegistry::RegisterComponents()
  -> GameplayTagRegistry::Initialise()
  -> GameStateMachine::Setup()
```

---

## Subsystem Scoping

| Scope | Current Base | Owner | Examples |
|-------|-------------|-------|---------|
| Engine services | (none - `EngineRuntime` monolith) | `EngineRuntime` | Window, Input, Time, RenderDevice, Renderer |
| Game services | `GameSubsystem` | `Game` via `SubsystemCollection` | GameplayTagRegistry, GameStateMachine, PhysicsSubsystem |
| Scene | (none) | `Scene` | Entity storage, component data |

**v2 changes:** Two formal scopes with base classes:
- `AppSubsystem` (application lifetime) - registered by plugins, dependency-ordered
- `StateSubsystem` (current state lifetime) - capability-gated, created/destroyed on state transitions

---

## Key Architectural Patterns

| Pattern | Where | Implementation |
|---------|-------|---------------|
| Composition over inheritance | Plugin system, ECS | Systems + components via flecs, not class hierarchies |
| Data-driven | Components, tags, scenes, config | TOML/JSON on disk, not hard-coded |
| Generational handles | Resource identity | `Handle<Tag>` with 20-bit index + 12-bit generation |
| Interned strings | Tags, pass names, keys | O(1) equality via pointer comparison |
| Type-erased events | Event dispatch | Per-type buffers + insertion order, double-buffered |
| Deferred dispatch | Events, state transitions | Queued during frame, processed at frame boundaries |
| Registrar pattern | Plugin declarations | Domain-specific registrars with validation |
| Result<T> error handling | Recoverable failures | `std::expected<T, Error>` alias throughout |
| RAII | Resource management | Subsystems, layers, plugins, GPU resources |
| Static accessor | ECS system callbacks | `GameSubsystems::Get<T>()` (flecs signature constraint) |
