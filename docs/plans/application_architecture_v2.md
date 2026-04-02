# Application Architecture v2

**Status:** Planned
**Related issues:** TBD
**Last updated:** 2026-04-02
**Supersedes:** [application_architecture_v1.md](application_architecture_v1.md)

---

## Goals

- **Engine is a library.** The game owns `main()`. `Application` is a type the game constructs.
- **Application is a minimal orchestrator.** Owns platform services, the plugin registry, the state machine, the overlay stack, and event routing. Its public API is `AddPlugin<T>()` and `Run()`.
- **Plugins are the sole unit of extension.** Everything flows through plugins: states, overlays, subsystems, ECS registrations, render features, gameplay tags. One composition model.
- **States and overlays are separate concerns.** Application states handle phase management (splash, menu, gameplay, editor). Overlays handle persistent or transient decorations (FPS counter, debug console, notifications). They have different lifecycles, different ownership semantics, and different relationships to each other.
- **Rendering is a set of composable services.** States and overlays submit to typed canvases (scene, UI, debug). The renderer collects all submissions into a single render graph per frame. Nothing outside the renderer touches the render graph directly.
- **Game is a standalone simulation engine.** Usable without Application, states, or a window. Headless tests, CLI tools, and the editor all use `Game` directly.
- **Editor and game use the same architecture.** Different plugin sets, same `Application` type. `EditorState` wraps `Game`, same as `GameplayState`.
- **Subsystems are scoped services.** Two lifetime scopes -- application and state -- with explicit dependency declaration and topological ordering.

---

## Changes from v1

| v1 | v2 | Rationale |
|---|---|---|
| Layers (one abstraction for states + overlays) | Separate Application States + Overlay stack | Layers conflated phase management with rendering decoration. States and overlays have different lifecycles, ownership, and relationships. |
| Three subsystem scopes (Engine, Game, Scene) | Two scopes (Application, State) | Scene-scoped subsystems are just ECS data and resources within the world. Two scopes cover all real needs. |
| `ApplicationContext` with `Publish<T>()` / `Find<T>()` | `EngineContext` wrapping two `SubsystemRegistry` instances | Everything is a subsystem. Renderer, Window, Input are app-scoped subsystems registered by plugins. Backend selection via plugin composition, not enums. |
| Plugin `OnStartup()` / `OnShutdown()` | Callback registration during `Build()` | Plugins hook specific lifecycle events they care about. No empty virtual methods. |
| `LayerStack` with transitions | `StateMachine` (hybrid: flat transitions + push/pop) | Explicit state machine with typed transitions, suspension semantics, and modal push/pop. |
| `PluginRegistry` stores declarations | `AppBuilder` passed to `Build()` | Renamed to reflect its role as a builder/wiring API, not a passive registry. |
| `GameLayer` wraps `Game` | `GameplayState` wraps `Game` | Terminology: application states, not layers. |
| Render features always active | Render features with `ValidStates` | Features activated/deactivated on state transitions. GPU resources persist (no churn). |
| Editor as overlay set | `EditorState` as an application state | Editor inverts the hierarchy: contains the game as a viewport, not an overlay on top of it. |
| `GameSubsystems` static accessor | `StateSubsystems` static accessor | Scope renamed from "Game" to "State" since any application state can have state-scoped subsystems. |

---

## Codebase Transition

This section maps existing engine code to v2 concepts, based on a thorough audit of the current codebase.

### What Gets Renamed

| Existing | v2 Name | Notes |
|---|---|---|
| `SubsystemCollection<TBase>` | `SubsystemRegistry<TBase>` | Same core design. Add dependency ordering and ValidStates. |
| `GameSubsystem` | `StateSubsystem` | Any application state can have state-scoped subsystems, not just Game. |
| `GameSubsystems` (static accessor) | `StateSubsystems` | Matches scope rename. Same Bind/Unbind pattern. |
| `PluginRegistry` | `AppBuilder` | Reflects its role as a builder/wiring API, not a passive registry. |
| `Plugin` | `IPlugin` | Interface naming convention (`I` prefix). |

### What Gets Kept (With Modifications)

| Existing | Changes |
|---|---|
| `SubsystemCollection<TBase>` (-> `SubsystemRegistry`) | Add `Initialise(SubsystemRegistry&)` signature for dependency resolution. Add topological sort for init/shutdown (reuse `SystemRegistrar`'s Kahn's algorithm pattern). Add ValidStates filtering for state-scoped subsystems. Retain existing predicate filtering and `ShouldCreate()`. |
| `SystemRegistrar` | Stays internal to `AppBuilder`. Topological sort pattern reused for subsystem dependency ordering. |
| `StateRegistrar` | Stays internal to `AppBuilder`. Duplicate detection retained. |
| `TagRegistrar` | Stays internal to `AppBuilder`. Declaration collector; feeds into `GameplayTagRegistry` at init. |
| `EventQueue` | No changes to the queue itself (double-buffered, typed storage, FIFO -- production quality). Dispatch order changes: overlays first (top-down), then active state. |
| `Game` | Remove `GameplayTagRegistry` ownership (TagRegistry moves to app-scoped). `SubsystemCollection<GameSubsystem>` becomes `SubsystemRegistry<StateSubsystem>`. Remove `GetTagRegistry()` (accessed via GameContext instead). Otherwise stays standalone. |
| `GameStateMachine` | Becomes `StateSubsystem`. Extends generic `StateMachine<TStateId>` base if feasible; otherwise built separately and extracted later. |
| `PhysicsSubsystem` | Becomes `StateSubsystem`. Gets ValidStates filtering (GameplayState + EditorState). |
| `GameplayTagRegistry` | Becomes `AppSubsystem` (tags are global knowledge, persist across state transitions). Accessible in Game flow via GameContext reference. |
| `GameContext` | Evolves to pull from `EngineContext` (adds AssetService, GameplayTagRegistry references). Game stays framework-agnostic. |
| `GameState.h` / `RunCondition` helpers | Unchanged. Works within ECS world. |
| `BlendableEffectRegistry` | Becomes a separate `AppSubsystem`. Game volumes push effects; renderer reads them. |
| `ProjectDescriptor` | Stays as stored data in EngineContext (structural metadata, not a service). Split into sub-structs if it grows. |

### What Gets Removed

| Existing | Reason |
|---|---|
| `Layer` / `LayerStack` | Replaced by `IApplicationState` + `IOverlay`. |
| `FpsOverlayLayer` | Rewritten as `FpsOverlay : IOverlay` using debug canvas API (current implementation only sets window title). |
| `PluginExport.h` / `PluginLoader.h` / `CreateGamePlugin()` | DLL plugin system designed for "engine loads game DLL" model. v2 has game owning `main()`. @todo: redesign for hot-reload and mod support when needed. |
| `Plugin::OnStartup()` / `Plugin::OnShutdown()` | Replaced by callback registration during `Build()`. |
| `EngineRuntime` | Fully decomposed into individual `AppSubsystem` instances (Window, Input, Time, Renderer, etc.). |
| `EngineConfig` (monolithic struct) | Decomposed into per-plugin config. See [Configuration Architecture](#configuration-architecture). |
| `EngineContext` (existing struct) | Replaced by new `EngineContext` class with SubsystemRegistries and state/overlay access. Not evolved -- the current 27-line non-owning struct is a fundamentally different concept. |
| `BackendConfig` / `PlatformBackend` / `RenderBackend` | Backend selection is plugin composition, not enum dispatch. Which plugins you add determines the backend. |
| `Window::Create()` / `Input::Create()` / `Time::Create()` static factories | Concrete types are constructed directly by their backend plugin (e.g. `SDLPlatformPlugin` creates `SDL3Window`). No factory indirection needed. |

### What Gets Added

| New | Purpose |
|---|---|
| `IApplicationState` | Rich state interface: OnEnter, OnExit, OnSuspend, OnResume, OnUpdate, OnRender, OnEvent + transition methods. |
| `IOverlay` | Lightweight decoration interface: OnAttach, OnDetach, OnUpdate, OnRender, OnEvent. |
| `ApplicationStateMachine` | Hybrid flat + push/pop state machine. Transition validation with `AllowDynamic<T>()` escape hatch. |
| `OverlayStack` | Ordered stack with persistent/transient overlays, ValidStates filtering, numeric input priority. |
| `AppSubsystem` | Base class for application-lifetime subsystems (Renderer, Window, etc.). |
| `EngineContext` (new class) | Central context with app + state SubsystemRegistries, active state access, overlay operations. |
| `GameplayState` | Wraps `Game` into `IApplicationState` lifecycle. |
| `StateMachine<TStateId>` | Generic state machine template shared by ApplicationStateMachine and GameStateMachine. |
| Config loader on `AppBuilder` | Cached TOML/JSON loading. `builder.LoadConfig<T>(section)`. @todo: hot-reload support. |

---

## Design Decisions

| Decision | Choice | Rationale |
|---|---|---|
| Entry point ownership | Game owns `main()` | Engine is a library. |
| Application scope | Minimal orchestrator | Owns EngineContext + AppBuilder + StateMachine + OverlayStack + EventQueue. Doesn't know about Game, Scene, or ECS. |
| Phase management | Application state machine | One active state at a time. Hybrid: flat transitions (replace) + push/pop (modal). |
| Overlay system | Separate ordered stack | Persistent or transient decorations. ValidStates for per-state filtering. Input top-down (numeric priority), render bottom-up. |
| Game wrapping | `GameplayState` wraps `Game` | Game stays standalone. GameplayState is the adapter into the state lifecycle. |
| ECS scope | flecs world inside `Game` | Not global. States and overlays that don't need ECS don't touch it. |
| Service access | `EngineContext` with subsystem registries | Two registries (app + state). No globals, no singletons except the bounded `StateSubsystems` static accessor for ECS callbacks. |
| Subsystem scopes | Application + State | Two scopes. Base class determines scope. ValidStates filters which states instantiate a state-scoped subsystem. |
| Subsystem naming | `SubsystemRegistry` (was `SubsystemCollection`) | "Registry" reflects registration + lookup + lifecycle management role. |
| Plugin interface | `Build(AppBuilder&)` + callback registration | No `OnStartup` / `OnShutdown`. Plugins register interest in specific lifecycle events. |
| Plugin dependencies | Explicit declaration + validation | AppBuilder validates and orders `Build()` calls. Clear errors on missing deps. |
| Sub-registrars | Internal to AppBuilder | `SystemRegistrar`, `StateRegistrar`, `TagRegistrar` stay as focused internal implementation details. |
| Rendering canvases | Fixed set (Scene, UI, Debug) | Designed internally for extensibility later. Public API for states and overlays. |
| Render feature lifecycle | Hybrid: Renderer owns, ValidStates activation | Existing `OnAttach`/`OnDetach`/`SetEnabled` unchanged. Features activate/deactivate on state transitions. GPU resources persist. |
| SceneRenderExtractor | Internal to Renderer | Implementation detail of scene submission processing. Designed for trivial extraction into a subsystem later if needed. |
| BlendableEffectRegistry | Separate AppSubsystem | Game volumes push effects, renderer reads them. Standalone subsystem makes this relationship explicit and testable. |
| Editor architecture | `EditorState` (same exe), extensible to separate exe later | `Game` is framework-agnostic. Both `GameplayState` and `EditorState` wrap it. |
| Editor panels vs overlays | Completely separate | Editor panels need docking, persistence, serialisation. Overlays are a simple stack. Different problems. |
| Shutdown ordering | Reverse dependency order | Topological sort for init, reverse for teardown. |
| State transitions | Direct method calls, validated with escape hatch | Declared transitions validated at startup. `AllowDynamic<T>()` escape hatch for states needing unrestricted transitions (e.g. mod support). |
| State internal state machines | Generic `StateMachine<TStateId>` template | Shared pattern: named states, transitions, enter/exit callbacks. Extract if feasible; build separately and extract later if not. |
| Configuration | Hybrid: AppBuilder config loader | Plugin-driven ownership (Bevy-like) with centralised caching. `builder.LoadConfig<T>(section)`. @todo: hot-reload. |
| GameplayTagRegistry scope | AppSubsystem | Tags are global knowledge. Persist across state transitions. Game accesses via GameContext. |
| ProjectDescriptor | Stored data in EngineContext | Structural metadata, not a service. Split into sub-structs if it grows. |
| Window event wiring | Application wires directly | Application gets Window via EngineContext, sets callback, routes events. Window is a subsystem but Application is the orchestrator. |
| DLL plugin system | Removed | Old "engine loads game DLL" model. @todo: redesign for hot-reload and mod support. |

---

## Architecture Overview

```
Application
  |
  +-- EngineContext
  |     +-- AppSubsystemRegistry       (application lifetime)
  |     |     +-- Renderer
  |     |     +-- Window
  |     |     +-- Input
  |     |     +-- Time
  |     |     +-- AssetService
  |     |     +-- TagRegistry
  |     |     +-- ...
  |     +-- StateSubsystemRegistry     (current state lifetime)
  |           +-- PhysicsSubsystem
  |           +-- AudioSubsystem
  |           +-- ...
  |
  +-- Plugins                          (owned permanently, Build() called once)
  +-- ApplicationStateMachine
  |     +-- [SplashState]              (initial; transitions to MainMenuState)
  |     +-- [MainMenuState]            (transitions to GameplayState)
  |     +-- [GameplayState]            (wraps Game; pushes PauseState)
  |     +-- [EditorState]              (wraps Game; renders to offscreen target)
  |     +-- [PauseState]               (pushed modal; pops back to GameplayState)
  |
  +-- OverlayStack
  |     +-- [FpsOverlay]               (persistent, all states)
  |     +-- [DebugConsoleOverlay]       (persistent, all states)
  |     +-- [GameHudOverlay]           (persistent, GameplayState only)
  |     +-- [AchievementToast]         (transient, pushed at runtime)
  |
  +-- EventQueue
  +-- ProjectDescriptor
  +-- EngineConfig
```

States and overlays are registered by plugins during `Build()`. Application doesn't know which states or overlays exist -- it just runs the state machine and overlay stack.

---

## Application State Machine

### Concept

The application state machine manages **phase-level** flow: splash screen, main menu, gameplay, editor. One state is active at a time. States own their update/render lifecycle and define what subsystems and overlays are valid within them.

This is distinct from game-level state machines (playing, loading, cutscene) which operate within `GameplayState` via `GameStateMachine` and ECS run conditions.

### Hybrid Model: Flat Transitions + Push/Pop

The state machine supports two operations:

- **Flat transition** (replace): The active state is torn down and replaced by a new state. `SplashState` -> `MainMenuState` -> `GameplayState`.
- **Push/pop** (modal): A new state is pushed on top, suspending the state below. `GameplayState` pushes `PauseState`. Pop returns to `GameplayState`. Depth-1 initially (one push at a time); extensible to deeper stacks later if needed.

### Push Configuration

When pushing a modal state, the pusher configures how the suspended state behaves:

```cpp
PushState<PauseState>({
    .AllowBackgroundRender = true,   // Game world visible (blurred) behind pause menu
    .AllowBackgroundUpdate = false,  // Game simulation pauses
});
```

| Flag | Effect |
|---|---|
| `AllowBackgroundRender = true` | Suspended state's `OnRender()` still called. Useful for rendering a blurred/dimmed game world behind a pause menu. |
| `AllowBackgroundRender = false` | Suspended state does not render. Pushed state owns the entire frame. |
| `AllowBackgroundUpdate = true` | Suspended state's `OnUpdate()` still called (e.g. ambient animations continue). |
| `AllowBackgroundUpdate = false` | Suspended state is frozen. No simulation tick. |

A pushed state can also trigger a flat transition, which replaces the entire stack (pops the modal and replaces the base state).

### State Interface

```cpp
class IApplicationState
{
public:
    virtual ~IApplicationState() = default;

    virtual void OnEnter(EngineContext& ctx) {}
    virtual void OnExit(EngineContext& ctx) {}
    virtual void OnSuspend(EngineContext& ctx) {}     // Another state pushed on top
    virtual void OnResume(EngineContext& ctx) {}      // Pushed state popped, we're active again
    virtual void OnUpdate(EngineContext& ctx, float deltaTime) {}
    virtual void OnRender(EngineContext& ctx) {}
    virtual void OnEvent(EngineContext& ctx, Event& event) {}
    virtual auto GetName() const -> std::string_view = 0;

protected:
    /// Replace the entire state stack with a new state. Deferred.
    template<std::derived_from<IApplicationState> T, typename... TArgs>
    void TransitionTo(TArgs&&... args);

    /// Push a modal state on top of this one. Deferred. Depth-1 initially.
    template<std::derived_from<IApplicationState> T, typename... TArgs>
    void PushState(PushStateConfig config, TArgs&&... args);

    /// Pop this state (if it was pushed). Deferred.
    void PopState();
};
```

### State Lifecycle

```
State registered by plugin during Build()
  |
  Application::Run() starts
  |
  Initial state enters: OnEnter(ctx)
  |
  Per frame:
    OnEvent(ctx, event)   -- input handling
    OnUpdate(ctx, dt)     -- simulation/logic
    OnRender(ctx)         -- submit to canvases
  |
  TransitionTo<NewState>() called (deferred):
    Current: OnExit(ctx)
    State subsystems torn down (reverse dependency order)
    State subsystems created for new state
    New: OnEnter(ctx)
  |
  PushState<ModalState>() called (deferred):
    Current: OnSuspend(ctx)
    Modal: OnEnter(ctx)
    [Modal runs; current optionally updates/renders per PushStateConfig]
  |
  PopState() called (deferred):
    Modal: OnExit(ctx)
    Resumed: OnResume(ctx)
```

All state operations are deferred: queued during the frame, processed at the start of the next frame before event dispatch.

### State Operations Summary

| Operation | When processed | Initiator |
|---|---|---|
| `TransitionTo<T>()` | Start of next frame | The active state itself |
| `PushState<T>(config)` | Start of next frame | The active state |
| `PopState()` | Start of next frame | The pushed/modal state |

### State Registration

States are registered by plugins during `Build()`:

```cpp
void Build(AppBuilder& builder) override
{
    builder.AddState<SplashState>({ .Initial = true });
    builder.AddState<MainMenuState>();
    builder.AddState<GameplayState>();

    builder.AddTransition<SplashState, MainMenuState>();
    builder.AddTransition<MainMenuState, GameplayState>();
}
```

Transitions are validated at startup: every `TransitionTo<T>()` must match a declared transition. Undeclared transitions are a startup error, not a runtime surprise.

States that need unrestricted transitions (e.g. for mod support or dynamic flow) can opt out:

```cpp
builder.AllowDynamic<GameplayState>();  // GameplayState can transition to any registered state
```

This is an escape hatch, not the default. Most states should declare their transitions explicitly -- the graph serves as documentation and catches typos early.

---

## Overlay System

### Concept

Overlays are lightweight decorations that render on top of the active state. FPS counters, debug consoles, achievement toasts, network disconnect notices. They don't own the frame -- the active state still drives update and render. Overlays add to it.

Overlays are **not** application states. They don't suspend anything. They don't own simulation. The distinction: "does this thing need to control whether the state below it keeps running?" If yes, it's a pushed state. If no, it's an overlay.

### Overlay Types

| Type | Lifecycle | Cleanup | Example |
|---|---|---|---|
| **Persistent** | Registered at startup by plugin | Survives state transitions. Auto-deactivated if invalid for current state. | FPS counter, debug console |
| **Transient** | Pushed at runtime by a state or another overlay | Cleaned up on state transition. | Achievement toast, loading spinner |

### Overlay Interface

```cpp
class IOverlay
{
public:
    virtual ~IOverlay() = default;

    virtual void OnAttach(EngineContext& ctx) {}
    virtual void OnDetach(EngineContext& ctx) {}
    virtual void OnUpdate(EngineContext& ctx, float deltaTime) {}
    virtual void OnRender(EngineContext& ctx) {}
    virtual void OnEvent(EngineContext& ctx, Event& event) {}
    virtual auto GetName() const -> std::string_view = 0;
};
```

### Overlay Registration

```cpp
void Build(AppBuilder& builder) override
{
    // Persistent: registered at startup, state-scoped
    builder.RegisterOverlay<FpsOverlay>({
        .ValidStates = {},                              // empty = all states
        .DefaultActive = true,
    });
    builder.RegisterOverlay<DebugConsoleOverlay>({
        .ValidStates = {},
        .DefaultActive = false,                         // toggled at runtime
        .InputPriority = 100,                           // higher = receives input first
    });
    builder.RegisterOverlay<GameHudOverlay>({
        .ValidStates = {StateId::Of<GameplayState>()},  // gameplay only
        .DefaultActive = true,
    });
}
```

### State Validity

Persistent overlays have a `ValidStates` set. When a state transition occurs:
- Overlays valid for the new state remain active (or become activatable).
- Overlays invalid for the new state are automatically deactivated.
- Overlays with empty `ValidStates` are valid in all states.

Transient overlays (pushed at runtime) are always cleaned up on state transition. If a notification needs to survive transitions, it should be a persistent overlay that gets activated/deactivated.

### Overlay Execution Order

- **Input**: top-down (highest priority first). Overlays can consume events to prevent them from reaching lower overlays or the active state.
- **Update**: all active overlays updated after the active state.
- **Render**: bottom-up (active state renders first, overlays render on top).

### Overlay Access to Game Data

Overlays that need game data (entity counts, system timings) access the active state through `EngineContext`:

```cpp
void PerformanceMetricsOverlay::OnRender(EngineContext& ctx)
{
    auto& debug = ctx.GetAppSubsystem<Renderer>().GetDebugCanvas();

    // Engine-level metrics: always available
    debug.DrawText(FormatFps(ctx.GetAppSubsystem<Time>().GetFps()), {.Position = topLeft});

    // Game-level metrics: only when gameplay is active
    if (auto* gameplay = ctx.FindActiveState<GameplayState>())
    {
        const auto& world = gameplay->GetGame().GetWorld();
        debug.DrawText(FormatEntityCount(world), {.Position = belowFps});
    }
}
```

`FindActiveState<T>()` returns `nullptr` if the active state isn't of that type. The dependency is explicit, visible, and null-safe.

---

## Application

### EngineContext

The single service-access mechanism for all states and overlays. No globals. No `Application::Get()`.

```cpp
class EngineContext
{
public:
    // -- Application-scoped subsystems --

    template<typename T>
    auto GetAppSubsystem() const -> T& { return m_appSubsystems.Get<T>(); }

    template<typename T>
    auto TryGetAppSubsystem() const -> T* { return m_appSubsystems.Try<T>(); }

    // -- State-scoped subsystems --

    template<typename T>
    auto GetStateSubsystem() const -> T& { return m_stateSubsystems.Get<T>(); }

    template<typename T>
    auto TryGetStateSubsystem() const -> T* { return m_stateSubsystems.Try<T>(); }

    // -- Convenience: searches state first, then app --

    template<typename T>
    auto TryGet() const -> T*;

    // -- Active state access (for overlays) --

    auto GetActiveState() -> IApplicationState*;

    template<typename T>
    auto FindActiveState() -> T* { return dynamic_cast<T*>(GetActiveState()); }

    // -- Overlay operations (for states and overlays at runtime) --

    void PushTransientOverlay(std::unique_ptr<IOverlay> overlay);
    void ActivateOverlay(OverlayId id);
    void DeactivateOverlay(OverlayId id);

    // -- Shutdown --

    void RequestStop();

private:
    SubsystemRegistry m_appSubsystems;
    SubsystemRegistry m_stateSubsystems;
    // ...
};
```

Passed to every state and overlay lifecycle method by reference.

### Application Class

```cpp
class Application
{
public:
    struct CommandLineArgs { int Count; char** Args; };

    explicit Application(const CommandLineArgs& args);
    ~Application();

    /// The sole public API for composition. Everything goes through plugins.
    template<std::derived_from<IPlugin> T, typename... TArgs>
    void AddPlugin(TArgs&&... args);

    void Run();

private:
    auto Initialise() -> Result<void>;
    void BuildPlugins();
    void Loop();
    void Shutdown();

    void OnEvent(Event& event);

    CommandLineArgs m_args{};
    std::unique_ptr<ProjectDescriptor> m_project;
    std::unique_ptr<EngineConfig> m_config;
    EngineContext m_context;
    ApplicationStateMachine m_stateMachine;
    OverlayStack m_overlayStack;
    EventQueue m_eventQueue;

    std::vector<std::unique_ptr<IPlugin>> m_plugins;
    bool m_running = false;
};
```

Application does **not** own or know about: `Game`, `Scene`, flecs, or any rendering calls beyond coordinating `OnRender` on the active state and overlays.

### Main Loop

```cpp
void Application::Loop()
{
    while (m_running)
    {
        auto& renderer = m_context.GetAppSubsystem<Renderer>();
        auto& time = m_context.GetAppSubsystem<Time>();

        time.BeginFrame();

        // 1. Process deferred state/overlay operations
        m_stateMachine.ProcessPending(m_context);
        m_overlayStack.ProcessPending(m_context);

        // 2. Drain batched input events
        //    Top-down: overlays first, then active state
        m_eventQueue.Drain([&](Event& e) {
            m_overlayStack.DispatchEvent(m_context, e);  // top-down, can consume
            if (not e.Handled)
                m_stateMachine.DispatchEvent(m_context, e);
        });

        const float dt = time.GetDeltaTime();

        // 3. Update: active state, then overlays
        m_stateMachine.Update(m_context, dt);
        m_overlayStack.Update(m_context, dt);

        // 4. Render: active state first, overlays on top
        renderer.BeginFrame();

        m_stateMachine.Render(m_context);   // state submits to canvases
        m_overlayStack.Render(m_context);   // overlays submit on top

        renderer.EndFrame();  // build graph, compile, execute, present

        time.EndFrame();
    }
}
```

The state machine handles background rendering of suspended states (if `AllowBackgroundRender` is set). Application just calls `m_stateMachine.Render()` and the SM knows whether to render the suspended state too.

---

## Game and GameplayState

### Game: Standalone Simulation Engine

`Game` owns the flecs world, subsystems, scenes, and the gameplay state machine. It is framework-agnostic: usable without `Application`, states, overlays, or a window.

```cpp
class Game
{
public:
    explicit Game(const Plugins::PluginRegistry& pluginRegistry);

    auto Initialise(const GameContext& ctx) -> Result<void>;
    void Update(float deltaTime);
    void Shutdown();

    auto GetWorld() -> flecs::world&;
    auto GetWorld() const -> const flecs::world&;
    auto GetCurrentScene() -> Scene*;
    auto GetStateMachine() -> GameStateMachine&;
    auto GetSubsystems() -> SubsystemRegistry<StateSubsystem>&;

    void LoadScene(std::string_view scenePath);
    void TransitionTo(std::string_view stateName);

private:
    flecs::world m_world;
    SubsystemRegistry<StateSubsystem> m_subsystems;
    std::unique_ptr<Scene> m_currentScene;
    GameStateMachine* m_stateMachine;
    const Plugins::PluginRegistry& m_pluginRegistry;
};
```

`GameContext` provides the bridge between EngineContext and Game, keeping Game framework-agnostic:

```cpp
struct GameContext
{
    const ProjectDescriptor& Project;
    const Plugins::PluginRegistry& PluginRegistry;
    AssetService* AssetService = nullptr;               // from EngineContext app subsystem
    GameplayTagRegistry* TagRegistry = nullptr;          // from EngineContext app subsystem
};
```

Headless tools and tests use `Game` directly:

```cpp
// Headless tool - no Application, no states, no window
auto project = ProjectDescriptor::Discover();
auto config = EngineConfig::Load(project);

Plugins::PluginRegistry registry(project, config);
JourneyContentPlugin plugin;
plugin.Build(registry);

GameplayTagRegistry tagRegistry;   // standalone, no subsystem infrastructure needed
AssetService assetService;

GameContext ctx{
    .Project = project,
    .PluginRegistry = registry,
    .AssetService = &assetService,
    .TagRegistry = &tagRegistry,
};
Game game(registry);
game.Initialise(ctx);
game.Update(1.0f / 60.0f);
```

### GameplayState: The Application Adapter

`GameplayState` wraps `Game` into the application state lifecycle. It is the bridge between the application framework and the simulation:

```cpp
class GameplayState : public IApplicationState
{
public:
    void OnEnter(EngineContext& ctx) override
    {
        // Create Game from plugin registry declarations
        m_game = std::make_unique<Game>(ctx.GetPluginRegistry());

        GameContext gameCtx{
            .Project = ctx.GetProject(),
            .PluginRegistry = ctx.GetPluginRegistry(),
            .AssetService = ctx.TryGetAppSubsystem<AssetService>(),
            .TagRegistry = ctx.TryGetAppSubsystem<GameplayTagRegistry>(),
        };
        m_game->Initialise(gameCtx);

        // Bind state subsystems for ECS access
        StateSubsystems::Bind(&m_game->GetSubsystems());
    }

    void OnExit(EngineContext& ctx) override
    {
        StateSubsystems::Unbind();
        m_game->Shutdown();
        m_game.reset();
    }

    void OnUpdate(EngineContext& ctx, float dt) override
    {
        m_game->Update(dt);
    }

    void OnRender(EngineContext& ctx) override
    {
        auto& renderer = ctx.GetAppSubsystem<Renderer>();

        // 3D scene through the scene pipeline
        if (const auto* scene = m_game->GetCurrentScene())
            renderer.SubmitScene(*scene);
    }

    void OnEvent(EngineContext& ctx, Event& event) override
    {
        // Forward input to game systems
    }

    auto GetGame() -> Game& { return *m_game; }
    auto GetGame() const -> const Game& { return *m_game; }
    auto GetName() const -> std::string_view override { return "GameplayState"; }

private:
    std::unique_ptr<Game> m_game;
};
```

### Game-Level State Machine

Game-level states (Playing, Loading, Cutscene, Dialogue) are handled by `GameStateMachine` inside `Game`. These control ECS system enable/disable via run conditions. They are separate from application states.

```cpp
// Plugin registers game states
void Build(AppBuilder& builder) override
{
    builder.RegisterState({
        .Name = "Playing",
        .OnEnter = [](flecs::world& world) { /* enable gameplay systems */ },
    });
    builder.RegisterState({
        .Name = "Loading",
        .OnEnter = [](flecs::world& world) { /* disable gameplay, start load */ },
    });
    builder.SetInitialState("Loading");
}
```

### Generic StateMachine Template

Application states and game states share a common pattern: named states, transitions, enter/exit callbacks, current/previous tracking. A thin generic template captures this:

```cpp
template<typename TStateId>
class StateMachine
{
public:
    void RegisterState(TStateId id, StateCallbacks<TStateId> callbacks);
    void TransitionTo(TStateId id);           // deferred
    auto GetCurrentState() const -> TStateId;
    auto GetPreviousState() const -> TStateId;
    void ProcessPending();

private:
    TStateId m_current;
    TStateId m_previous;
    std::optional<TStateId> m_pendingTransition;
    // ...
};
```

`GameStateMachine` extends this with ECS-specific functionality: run conditions, system binding, world reference. `ApplicationStateMachine` extends with typed C++ state objects, push/pop semantics, and the rich lifecycle (OnEnter/OnExit/OnSuspend/OnResume/OnUpdate/OnRender/OnEvent).

---

## Subsystems

### Concept

Subsystems are **scoped services** with managed lifetimes. They provide capabilities (physics, audio, rendering, asset management) that states, overlays, and ECS systems consume. A subsystem is a **service**: it has behaviour, manages state, provides an API. Data belongs in ECS components, assets, or config structs.

| Should be a subsystem | Should NOT be a subsystem |
|---|---|---|
| Physics engine wrapper | Individual physics bodies (ECS components) |
| Gameplay tag registry | A specific tag (data) |
| Asset manager | A specific loaded asset |
| Navigation/pathfinding service | A specific nav mesh (asset/ECS data) |
| Audio engine | A playing sound instance |

### Two Scopes

| Scope | Base class | Owner | Lifetime | Examples |
|---|---|---|---|---|
| Application | `AppSubsystem` | `EngineContext` | Application start to stop | Renderer, Window, Input, Time, AssetService, TagRegistry |
| State | `StateSubsystem` | `EngineContext` (state registry) | State enter to state exit | PhysicsSubsystem, AudioSubsystem, NavigationSubsystem |

The base class determines scope. Concept-constrained overloads route registration automatically:

```cpp
template<std::derived_from<AppSubsystem> T>
void RegisterSubsystem(AppSubsystemConfig config = {});

template<std::derived_from<StateSubsystem> T>
void RegisterSubsystem(StateSubsystemConfig config = {});
```

When `PhysicsSubsystem` derives from `StateSubsystem`, the second overload matches. When `Renderer` derives from `AppSubsystem`, the first matches. Adding a new scope = new base class + new overload.

### Abstract-Type Resolution

Consumers access subsystems by their abstract interface type (`ctx.GetAppSubsystem<Window>()`), not by the concrete backend type. When a plugin registers `SDL3Window`, the registry stores the instance under both `typeid(SDL3Window)` and `typeid(Window)`. A `Get<Window>()` call resolves to the SDL3Window without the caller knowing or caring which backend is active.

This is a small extension to `SubsystemRegistry`'s `Register<T>()`: walk `T`'s public `AppSubsystem` / `StateSubsystem` base classes and insert additional lookup entries for each abstract ancestor. The concrete entry remains the canonical owner; abstract entries are non-owning aliases. Duplicate abstract registrations (two plugins both registering a `Window`) are a startup validation error.

### State-Scoped Subsystem Filtering (ValidStates)

State-scoped subsystems specify which application states they're relevant to:

```cpp
builder.RegisterSubsystem<PhysicsSubsystem>({
    .ValidStates = {StateId::Of<GameplayState>(), StateId::Of<EditorState>()},
    .Config = PhysicsConfig{ .Gravity = {0.f, -9.81f, 0.f} },
});
```

When `GameplayState` enters, `PhysicsSubsystem` is constructed. When `SplashState` enters, it's skipped. When transitioning from `GameplayState` to `MainMenuState`, the subsystem is destroyed and recreated on next entry (fresh state).

Empty `ValidStates` = available in all states (rare for state-scoped, but possible).

### Dependency Declaration

Subsystems declare their dependencies explicitly:

```cpp
class PhysicsSubsystem : public StateSubsystem
{
public:
    void Initialise(SubsystemRegistry& registry) override
    {
        // Explicit: I need this or I can't function
        m_assetManager = &registry.Get<AssetService>();
    }

    void Shutdown() override { /* release resources */ }

private:
    AssetService* m_assetManager = nullptr;
};
```

The registry validates the dependency graph at startup. Missing dependencies produce a clear error ("PhysicsSubsystem requires AssetService but it wasn't registered") rather than a runtime null deref. Dependencies enable topological ordering: initialisation follows dependency order, shutdown follows reverse dependency order.

### Subsystem Lifecycle on State Transition

```
State transition: GameplayState -> MainMenuState
  |
  1. State subsystems for GameplayState shut down (reverse dependency order)
  2. StateSubsystemRegistry cleared
  3. State subsystems for MainMenuState constructed (per ValidStates)
  4. State subsystems initialised (dependency order)
  5. StateSubsystems static accessor rebound
  6. MainMenuState::OnEnter() called
```

### Static Accessor for ECS Systems

flecs system callbacks are lambdas with component signatures controlled by flecs. They cannot receive injected references. `StateSubsystems` provides a bounded static accessor:

```cpp
world.system<Transform, RigidBody>()
    .each([](Transform& t, RigidBody& rb) {
        auto& physics = StateSubsystems::Get<PhysicsSubsystem>();
        physics.SyncTransform(t, rb);
    });
```

The scope is strictly bounded: bound on state enter, unbound on state exit. Lambda capture is the preferred alternative when practical (e.g. in `GameplayState::OnEnter` setting up systems with captured references).

### Subsystems vs ECS World Singletons

These are orthogonal mechanisms serving different purposes:

| | Subsystems | World Singletons |
|---|---|---|
| Nature | **Services** with behaviour and APIs | **Data** that ECS systems read/write |
| Examples | PhysicsSubsystem, AudioSubsystem | ActiveGameState, WeatherData, SceneSettings |
| Access | `StateSubsystems::Get<T>()` or `ctx.GetStateSubsystem<T>()` | `world.get<T>()` or queried in system signatures |
| Lifecycle | Managed by subsystem registry | Managed by ECS world |

Plugins register both. Use both where appropriate.

---

## Plugins

### Concept

Plugins are the **sole unit of extension**. Everything the application needs -- states, overlays, subsystems, ECS registrations, render features, gameplay tags -- is provided by plugins. `Application::AddPlugin<T>()` is the only public API.

### Plugin Interface

```cpp
class IPlugin
{
public:
    virtual ~IPlugin() = default;

    /// Declare everything this plugin provides. Called once during Application::Initialise().
    virtual void Build(AppBuilder& builder) = 0;

    virtual auto GetName() const -> std::string_view = 0;
};
```

No `OnStartup()` or `OnShutdown()`. Plugins register interest in specific lifecycle events during `Build()`:

```cpp
void Build(AppBuilder& builder) override
{
    // Register subsystems
    builder.RegisterSubsystem<PhysicsSubsystem>({
        .ValidStates = {StateId::Of<GameplayState>()},
        .Config = PhysicsConfig{ .Gravity = {0.f, -9.81f, 0.f} },
    });

    // Hook specific lifecycle events
    builder.OnStateEnter<GameplayState>([](EngineContext& ctx) {
        auto& physics = ctx.GetStateSubsystem<PhysicsSubsystem>();
        physics.LoadCollisionData("physics/world.col");
    });

    // App-level one-time setup after all app subsystems are live
    builder.OnAppReady([](EngineContext& ctx) {
        // Cross-plugin wiring, validation
    });
}
```

Benefits:
- A plugin only hooks the events it cares about (no empty virtual methods).
- Per-state targeting: hook `GameplayState` enter but not `SplashState`.
- Multiple callbacks per event.
- The system knows at startup exactly which plugins care about which events.

### Plugin Dependencies

Plugins declare dependencies. The registry validates and orders `Build()` calls:

```cpp
class PhysicsPlugin : public IPlugin
{
public:
    void Build(AppBuilder& builder) override
    {
        builder.DependsOn<CorePlugin>();  // CorePlugin::Build() runs first

        builder.RegisterSubsystem<PhysicsSubsystem>({...});
    }

    auto GetName() const -> std::string_view override { return "Physics"; }
};
```

Missing dependencies produce a startup error. Circular dependencies are detected and rejected.

### What Plugins Register

| Registration | Method | Scope |
|---|---|---|
| Application states | `AddState<T>()` | Application SM |
| State transitions | `AddTransition<A, B>()` | Application SM |
| Overlays | `RegisterOverlay<T>()` | Overlay stack |
| App subsystems | `RegisterSubsystem<T : AppSubsystem>()` | Application lifetime |
| State subsystems | `RegisterSubsystem<T : StateSubsystem>()` | State lifetime (per ValidStates) |
| Render features | `RegisterRenderFeature<T>()` | Renderer (per ValidStates) |
| ECS systems | `RegisterSystem()` | Game (flecs world) |
| ECS components | `RegisterComponent()` | Game (flecs world) |
| ECS globals | `RegisterGlobal()` | Game (flecs world) |
| Game states | `RegisterState()` | GameStateMachine |
| Gameplay tags | `RegisterTag()` / `RegisterTagFile()` | TagRegistry |
| Sub-plugins | `AddPlugin<T>()` | Mixed |
| Lifecycle hooks | `OnAppReady()`, `OnStateEnter<T>()`, etc. | Callbacks |

### Backend Selection via Plugins

Core engine services are subsystems registered by plugins. Backend selection is plugin composition, not configuration enums:

```cpp
// Game main()
app.AddPlugin<VulkanRendererPlugin>();   // registers VulkanRenderer : Renderer : AppSubsystem
app.AddPlugin<SDLWindowPlugin>();        // registers SDLWindow : Window : AppSubsystem
app.AddPlugin<SDLInputPlugin>();         // registers SDLInput : Input : AppSubsystem
app.AddPlugin<JourneyGamePlugin>();
app.Run();

// Headless tool
app.AddPlugin<NullRendererPlugin>();     // registers NullRenderer : Renderer : AppSubsystem
app.AddPlugin<HeadlessWindowPlugin>();
app.Run();
```

Different executable? Different plugin set. The plugin set IS the application's capability manifest.

### Plugin Composition Patterns

**Content vs presentation:**

```cpp
// Journey's default: splash -> game
app.AddPlugin<JourneyContentPlugin>();  // systems, components, subsystems (no states)
app.AddPlugin<SplashPlugin>();          // adds SplashState (transitions to GameplayState)
app.AddPlugin<FpsOverlayPlugin>();      // adds FPS overlay

// Headless testing: game content, no presentation
app.AddPlugin<JourneyContentPlugin>();
app.AddPlugin<HeadlessPlugin>();        // adds GameplayState with null renderer

// Editor: game content + editor presentation
app.AddPlugin<JourneyContentPlugin>();
app.AddPlugin<EditorPlugin>();          // adds EditorState + editor panels
```

**Plugin groups:**

```cpp
struct JourneyDefaultPlugins : IPlugin
{
    void Build(AppBuilder& builder) override
    {
        builder.AddPlugin<VulkanRendererPlugin>();
        builder.AddPlugin<SDLWindowPlugin>();
        builder.AddPlugin<JourneyContentPlugin>();
        builder.AddPlugin<SplashPlugin>();
        builder.AddPlugin<FpsOverlayPlugin>();
    }

    auto GetName() const -> std::string_view override { return "JourneyDefaults"; }
};

// main() becomes:
app.AddPlugin<JourneyDefaultPlugins>();
app.Run();
```

---

## Configuration Architecture

### Problem: Monolithic EngineConfig

The existing `EngineConfig` struct bundles `WindowConfig`, `BackendConfig`, `ShaderConfig`, `PhysicsConfig` into one type. This grows with every engine feature, loads config for modules that may not be loaded, and doesn't support per-plugin modularity.

### Solution: Plugin-Driven Config with Cached Loading

Each plugin owns its config types. `AppBuilder` provides a cached config loader during `Build()`:

```cpp
void PhysicsPlugin::Build(AppBuilder& builder) override
{
    auto config = builder.LoadConfig<PhysicsConfig>("physics");

    builder.RegisterSubsystem<PhysicsSubsystem>({
        .ValidStates = {StateId::Of<GameplayState>()},
        .Config = std::move(config),
    });
}
```

**How it works:**
- Project config files live in the project's config directory (e.g. `config/physics.toml`, `config/rendering.toml`).
- `builder.LoadConfig<T>(section)` reads, parses, and caches the file. Multiple plugins reading the same file parse once.
- Config structs are per-subsystem/per-plugin. No global `EngineConfig` monolith.
- If a plugin isn't loaded, its config is never read.
- Subsystems receive config through their registration descriptor, not by runtime lookup.

This follows Bevy's plugin-local ownership model (each plugin owns its config types) combined with Unreal/Godot's central caching (single point for file I/O and parsing).

### @todo: Future Extensions

**Hot-reload.** The config loader tracks which files and sections were loaded. A future file watcher can re-parse changed sections and push updates to subsystems that implement `OnConfigReloaded()`. The centralised tracking in AppBuilder makes this straightforward to add.

**Runtime querying.** If a subsystem needs to read config at runtime (not just at Build() time), a lightweight `ConfigService` AppSubsystem can be added that exposes the cached data. This is a future extension, not needed initially.

---

## Rendering Architecture

### Principle: Rendering as Composable Services

`EngineRuntime` becomes a set of app-scoped subsystems. The `Renderer` doesn't know about scenes. States and overlays submit to typed canvases. The render graph is an internal implementation detail.

### Decomposed Services

The existing `EngineRuntime` monolith is fully decomposed. Each service becomes an `AppSubsystem` registered by a backend plugin:

| Service | Becomes | Registered by |
|---|---|---|
| Window | `AppSubsystem` | `SDLWindowPlugin` |
| Input | `AppSubsystem` | `SDLInputPlugin` |
| Time | `AppSubsystem` | Core engine plugin |
| Renderer | `AppSubsystem` | `VulkanRendererPlugin` |
| BlendableEffectRegistry | `AppSubsystem` (standalone) | Rendering plugin |
| SceneRenderExtractor | Internal to Renderer | N/A (implementation detail) |

`BlendableEffectRegistry` is a separate `AppSubsystem` because game volumes push effects and the renderer reads them -- both sides access it independently. `SceneRenderExtractor` stays internal to the Renderer (converts Scene data to GPU-ready structures during `SubmitScene()`), but is designed so it can be trivially extracted into its own subsystem later if needed.

### Canvases

The Renderer exposes a fixed set of typed canvases that collect commands into flat buffers. Each canvas is backed by a `RenderFeature` that reads the buffer during `AddPasses()` and contributes to the render graph.

| Canvas | Backed by | RenderPhase | Used by |
|---|---|---|---|
| Scene submission | Scene pipeline features (opaque, post-process, composition) | PreOpaque through Composite | GameplayState, EditorState |
| UI canvas | UI render feature | Overlay | Any state (splash, menu, HUD) |
| Debug canvas | Debug render feature | Overlay | Debug overlays, editor gizmos |

These are the public API. States and overlays never touch `RenderDevice` or `RenderGraph` directly.

The canvas set is fixed; the internals are designed so adding a new canvas type is straightforward if needed later.

### Render Features: Hybrid Lifecycle

Render features are owned by the `Renderer` and activated/deactivated based on application state. This maps onto the existing `RenderFeature` system with minimal changes.

**What exists today:**
- `RenderFeature` base with `OnAttach()` / `OnDetach()` / `SetEnabled()` / `AddPasses()`
- `RenderOrchestrator` stores features in phase-ordered `FeatureSlot` vector
- `BuildGraph()` skips disabled features
- `RegisterDefaultFeatures()` adds all features unconditionally at startup

**What changes:**
1. Feature registration moves from `RegisterDefaultFeatures()` into plugins.
2. Each feature gets a `ValidStates` config (stored in `FeatureSlot`).
3. Application notifies Renderer on state transitions.
4. Renderer calls `SetEnabled()` based on `ValidStates`.
5. Features are created once at startup (GPU resources persist), just enabled/disabled per state.

```cpp
void Build(AppBuilder& builder) override
{
    builder.RegisterRenderFeature<SSAOFeature>({
        .ValidStates = {StateId::Of<GameplayState>(), StateId::Of<EditorState>()},
        .Phase = RenderPhase::PostOpaque,
        .Order = 0,
        .Config = SSAOConfig{ .Radius = 0.5f, .Samples = 16 },
    });
}
```

**Always-on features** (composition pass, present pass) use empty `ValidStates` and are always enabled.

### Rendering Data Flow

```
Application::Loop()
  |
  renderer.BeginFrame()                    clear canvases
  |
  GameplayState::OnRender()
  |   renderer.SubmitScene(*scene)         feed scene data to scene features
  |   renderer.GetUICanvas()               push HUD commands
  |
  GameHudOverlay::OnRender()
  |   renderer.GetUICanvas()               push health bar, minimap
  |
  FpsOverlay::OnRender()
  |   renderer.GetDebugCanvas()            push FPS text
  |
  renderer.EndFrame()
      |
      RenderOrchestrator::BuildGraph()
      |   scene features -> AddPasses      (opaque, post-process, composite)
      |   UI feature -> AddPasses          (screen-space draw commands)
      |   debug feature -> AddPasses       (debug text, gizmos)
      |   present feature -> AddPasses     (swapchain blit)
      |
      RenderGraph::Compile()               topological sort, dead-pass cull
      RenderGraph::Execute()               GPU work
      present
```

### State Rendering Examples

```cpp
void GameplayState::OnRender(EngineContext& ctx)
{
    auto& renderer = ctx.GetAppSubsystem<Renderer>();

    if (const auto* scene = m_game->GetCurrentScene())
        renderer.SubmitScene(*scene);
}

void SplashState::OnRender(EngineContext& ctx)
{
    auto& ui = ctx.GetAppSubsystem<Renderer>().GetUICanvas();
    ui.DrawSprite(m_logo, {.Position = m_centre, .Opacity = m_fadeAlpha});
}

void FpsOverlay::OnRender(EngineContext& ctx)
{
    auto& debug = ctx.GetAppSubsystem<Renderer>().GetDebugCanvas();
    debug.DrawText(m_fpsString, {.Position = topLeft});
}
```

States and overlays never touch `RenderDevice`. They submit to typed canvases. The Renderer handles the rest.

### Dynamic Pipeline Composition

Scene-related render features (opaque pass, post-processing) have `ValidStates` scoped to `GameplayState` and `EditorState`. When `SplashState` is active, those features are disabled -- only UI and present features contribute to the graph. Lightweight rendering, no wasted work.

---

## Editor Architecture

### EditorState

The editor is an application state, not an overlay. It **inverts** the hierarchy: `EditorState` contains the game as a viewport, rather than overlaying on top of it.

```
GameplayState:
  [Game renders to swapchain directly]

EditorState:
  [Game renders to offscreen framebuffer]
  [Editor UI renders to swapchain, embedding game framebuffer as a texture]
```

`EditorState` wraps the same `Game` class as `GameplayState`, but renders it to an offscreen target and composites it into an editor viewport alongside panels.

```cpp
class EditorState : public IApplicationState
{
public:
    void OnEnter(EngineContext& ctx) override
    {
        m_game = std::make_unique<Game>(ctx.GetPluginRegistry());
        m_game->Initialise(BuildGameContext(ctx));
        m_panels = std::make_unique<EditorPanelManager>();
    }

    void OnUpdate(EngineContext& ctx, float dt) override
    {
        if (m_playMode == PlayMode::Playing)
            m_game->Update(dt);

        m_panels->Update(ctx, dt);
    }

    void OnRender(EngineContext& ctx) override
    {
        auto& renderer = ctx.GetAppSubsystem<Renderer>();

        // 1. Render game to offscreen target
        if (const auto* scene = m_game->GetCurrentScene())
            renderer.SubmitScene(*scene, m_offscreenTarget);

        // 2. Draw editor panels (viewport embeds the offscreen texture)
        m_panels->Render(ctx);
    }

    auto GetGame() -> Game& { return *m_game; }
    auto GetName() const -> std::string_view override { return "EditorState"; }

private:
    std::unique_ptr<Game> m_game;
    std::unique_ptr<EditorPanelManager> m_panels;
    PlayMode m_playMode = PlayMode::Paused;
};
```

### EditorPanelManager

A thin layout orchestrator (~200 lines) within `EditorState`. Handles docking, panel registration, and layout persistence. Each panel renders using the shared UI toolkit.

```
Shared UI Toolkit (engine subsystem)
  +-- Used by: Game HUD, menus, splash screens (via UICanvas)
  +-- Used by: Editor panels (via EditorPanelManager)
  +-- Used by: Debug overlays

EditorPanelManager (EditorState-internal)
  +-- Docking layout
  +-- Panel registration
  +-- Layout serialisation
  +-- Each panel renders using the shared UI toolkit
```

The UI toolkit is a shared engine service. The `EditorPanelManager` is just the docking/arrangement layer. Both game UI and editor panels use the same rendering primitives. This follows Unreal (Slate for both) and Godot (Control nodes for both).

Editor panels are **not** overlays. They're managed entirely within `EditorState`. Game overlays (FPS counter, debug console) are the Application-level overlay stack and render on top of everything, including the editor.

### Editor vs Game: Same Architecture

```cpp
// Game main()
app.AddPlugin<VulkanRendererPlugin>();
app.AddPlugin<SDLWindowPlugin>();
app.AddPlugin<JourneyContentPlugin>();
app.AddPlugin<SplashPlugin>();       // SplashState -> transitions to GameplayState
app.AddPlugin<FpsOverlayPlugin>();
app.Run();

// Editor main()
app.AddPlugin<VulkanRendererPlugin>();
app.AddPlugin<SDLWindowPlugin>();
app.AddPlugin<JourneyContentPlugin>();
app.AddPlugin<EditorPlugin>();       // EditorState (initial)
app.AddPlugin<FpsOverlayPlugin>();   // Same overlay, works in both
app.Run();
```

Same `Application` type, same engine, different plugin sets. The editor is not special architecture.

### Future: Separate Executable

`Game` is framework-agnostic. Both `GameplayState` and `EditorState` wrap it. A future separate editor executable can also wrap `Game` directly. The current design (Option A: same exe) does not preclude adding a separate-exe path (Option B) later. The key invariant is that `Game` never depends on the application framework.

---

## Ownership Summary

| Concept | Owner | Accessed via |
|---|---|---|
| Platform subsystems (Window, Input, Time, Renderer) | EngineContext (app subsystem registry) | `ctx.GetAppSubsystem<T>()` |
| State-scoped subsystems (Physics, Audio) | EngineContext (state subsystem registry) | `ctx.GetStateSubsystem<T>()` or `StateSubsystems::Get<T>()` |
| Application state machine | Application | States call transition methods directly |
| Overlay stack | Application | States/overlays use `ctx.PushTransientOverlay()` etc. |
| Event queue | Application | Dispatched by Application each frame |
| Project/config | Application | Through EngineContext or AppBuilder |
| Plugins | Application (permanent) | `Build()` called once at init |
| ECS world, scenes, game SM | Game (owned by GameplayState/EditorState) | `FindActiveState<GameplayState>()->GetGame()` |

---

## Lifetime Boundaries

| Scope | Lifetime | What lives here |
|---|---|---|
| Application | App start to app stop | Renderer, Window, Input, Time, AssetService, TagRegistry, plugins, state machine, overlay stack |
| State | State enter to state exit | PhysicsSubsystem, AudioSubsystem, NavigationSubsystem, and any other state-scoped subsystems matching ValidStates |

Overlay-scoped subsystems don't exist. Overlays are lightweight and shouldn't own services. Scene-scoped is just ECS data and resources within the world; making it a subsystem scope adds indirection for no gain.

---

## Shutdown and Teardown

### State Transition Teardown

When transitioning between states:

1. Active state's `OnExit()` called.
2. State-scoped subsystems shut down in **reverse dependency order** (topological sort).
3. State subsystem registry cleared.
4. `StateSubsystems` accessor unbound.
5. Render features deactivated (SetEnabled(false)) for features not valid in the new state.
6. New state's state-scoped subsystems constructed and initialised (dependency order).
7. `StateSubsystems` accessor rebound.
8. Render features activated for the new state.
9. New state's `OnEnter()` called.

### Application Shutdown

1. `Application::RequestStop()` sets `m_running = false`.
2. Main loop exits.
3. Active state's `OnExit()` called.
4. State-scoped subsystems shut down (reverse dependency order).
5. Overlays detached.
6. App-scoped subsystems shut down (reverse dependency order).
7. Plugins destroyed (reverse registration order).

---

## Appendix: What This Architecture Does NOT Cover

These are separate subsystems or future work that this document intentionally leaves out:

- **UI toolkit specifics**: Widget types, layout systems, text rendering, input focus. The architecture provides `UICanvas` as the submission interface; the toolkit itself is a separate design.
- **Asset pipeline**: Loading, caching, hot-reload, reference counting. `AssetService` is an app subsystem; its internals are separate.
- **Networking**: Multiplayer, replication, state sync. Would be a state-scoped subsystem.
- **Audio**: Engine integration. Would be a state-scoped subsystem accessing the audio device (app-scoped).
- **Scripting**: If added, scripts are ECS systems registered by plugins.
- **Serialisation**: Scene format, component serialisation. Orthogonal to application architecture.
