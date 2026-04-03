# Application Architecture v2

**Status:** Planned
**Related issues:** TBD
**Last updated:** 2026-04-03
**Supersedes:** [application_architecture_v1.md](application_architecture_v1.md)

**Companion documents:**
- [application_migration_v2.md](application_migration_v2.md) - Codebase transition tables (rename/keep/remove/add)
- [game_framework.md](game_framework.md) - Simulation, GameplayState, EditorState, service providers, sub-state machines, IStateUI usage
- [rendering_performance.md](rendering_performance.md) - ECS-to-GPU hot path goals
- [console.md](console.md) - Debug console: commands, cvars, overlay, editor panel

---

## Scope

This document covers the application shell: how plugins compose an application, how states and overlays manage phases and decorations, how subsystems provide scoped services, and how rendering integrates as a set of composable services.

**Not covered here:** UI toolkit internals, asset pipeline, networking, audio, scripting, serialisation. These are separate subsystems that plug into this architecture.

---

## Goals

- **Engine is a library.** The game owns `main()`. `Application` is a type the game constructs.
- **Application is a minimal orchestrator.** Owns platform services, the state machine, the overlay stack, and event routing. Public API: `AddPlugin<T>()` and `Run()`.
- **Plugins are the sole unit of extension.** States, overlays, subsystems, ECS registrations, render features, gameplay tags - everything flows through plugins.
- **States and overlays are separate concerns.** States manage phases (splash, menu, gameplay). Overlays are persistent decorations (FPS counter, debug console). Different lifecycles, different ownership.
- **Game is a standalone simulation engine.** See [game_framework.md](game_framework.md). Renamed to `Simulation` -- owns the flecs world and scene management, nothing else.
- **Subsystems are scoped services.** Application and state lifetime, with dependency declaration and topological ordering.
- **Threading-ready from day one.** Interfaces designed so parallelism can be introduced incrementally. Frame data isolated, shared mutable state minimised.

---

## Architecture Overview

```
Application
  |
  +-- EngineContext
  |     +-- AppSubsystemRegistry       (application lifetime)
  |     |     +-- Renderer, Window, Input, Time, AssetService, TagRegistry, ...
  |     +-- StateSubsystemRegistry     (current state lifetime)
  |           +-- PhysicsSubsystem (requires: Simulation), AudioSubsystem, ...
  |
  +-- Plugins                          (owned permanently, Build() called once)
  +-- ApplicationStateMachine
  |     +-- [SplashState]              (provides: Presentation)
  |     +-- [GameplayState]            (provides: Simulation, Rendering, Presentation)
  |     +-- [EditorState]              (provides: Simulation, Rendering, Editing)
  |     +-- [PauseState]              (pushed modal, provides: Presentation)
  |
  +-- OverlayStack
  |     +-- [FpsOverlay]               (persistent, no capability requirement)
  |     +-- [DebugConsoleOverlay]      (persistent, no capability requirement, toggled at runtime)
  |     +-- [RenderGraphOverlay]       (persistent, requires: Rendering)
  |
  +-- EventQueue
  +-- ProjectDescriptor
```

States and overlays are registered by plugins. Application doesn't know which ones exist -- it just runs the state machine and overlay stack.

---

## Capability System

Capabilities are the **single activation mechanism** for subsystems, overlays, and render features. They are `GameplayTag` values -- the engine's hierarchical tag system built on `InternedString` (O(1) equality). Plugins can freely define new tags without modifying a central enum.

```cpp
namespace Capability
{
    inline const GameplayTag Simulation{"Capability.Simulation"};
    inline const GameplayTag Rendering{"Capability.Rendering"};
    inline const GameplayTag Presentation{"Capability.Presentation"};
    inline const GameplayTag Editing{"Capability.Editing"};
}
```

### Two Sources, One Namespace

| Source | Lifetime | Examples | Provider |
|---|---|---|---|
| **App-level** | Application lifetime | `Capability.Rendering.GPU`, `Capability.Platform.Window` | Plugins, based on hardware/config |
| **State-level** | Current state lifetime | `Capability.Simulation`, `Capability.Presentation` | States, declared at registration |

The **effective capability set** = app capabilities + active state capabilities.

```cpp
// Plugin provides app-level capabilities
void RenderingPlugin::Build(AppBuilder& builder)
{
    builder.ProvideCapability(Capability::Rendering::GPU);
    builder.RegisterAppSubsystem<Renderer>({
        .RequiredCapabilities = { Capability::Rendering::GPU },
    });
}

// States provide state-level capabilities
builder.AddState<GameplayState>({
    .Capabilities = { Capability::Simulation, Capability::Rendering, Capability::Presentation },
});
```

### Activation Rules

Anything that declares `RequiredCapabilities` is activated when the effective set satisfies its requirements. This applies uniformly to:

- **State-scoped subsystems**: constructed/destroyed on state transition
- **Persistent overlays**: activated/deactivated on state transition
- **Render features**: enabled/disabled via `SetEnabled()` on state transition
- **App-scoped subsystems**: checked once at startup (e.g. no GPU = no Renderer)

Empty `RequiredCapabilities` = always active. Adding a new state (e.g. `ReplayState` providing `Simulation + Rendering`) automatically activates all the right subsystems, overlays, and render features without touching any other plugin.

```cpp
builder.RegisterSubsystem<PhysicsSubsystem>({
    .RequiredCapabilities = { Capability::Simulation },
});
builder.RegisterOverlay<RenderGraphOverlay>({
    .RequiredCapabilities = { Capability::Rendering },
});
builder.RegisterRenderFeature<SSAOFeature>({
    .RequiredCapabilities = { Capability::Rendering },
});
```

---

## Application State Machine

### Concept

Manages **phase-level** flow: splash screen, main menu, gameplay, editor. One state is active at a time.

Individual application states may own **sub-state machines** for internal flow (e.g. GameplayState: Playing/Loading/Cutscene; MainMenuState: TitleScreen/SaveSelect/Settings). These are generic `StateMachine<InternedString>` instances held as member variables of the owning `IApplicationState`, not engine-managed. Sub-states are registered by plugins via `builder.ForState<T>()` and control ECS system activation through `RunCondition` helpers reading the `ActiveGameState` world singleton. See [game_framework.md](game_framework.md) for sub-state registration details.

### State Interface

```cpp
class IApplicationState
{
public:
    virtual ~IApplicationState() = default;

    virtual auto OnEnter(EngineContext& ctx) -> Result<void> { return {}; }
    virtual void OnExit(EngineContext& ctx) {}
    virtual void OnSuspend(EngineContext& ctx) {}
    virtual void OnResume(EngineContext& ctx) {}
    virtual void OnUpdate(EngineContext& ctx, float deltaTime) {}
    virtual void OnRender(EngineContext& ctx) {}
    virtual void OnEvent(EngineContext& ctx, Event& event) {}
    virtual auto GetName() const -> std::string_view = 0;

    virtual auto GetBackgroundPreferences() const -> BackgroundPreferences { return {}; }
    virtual auto GetSuspensionPolicy() const -> SuspensionPolicy { return {}; }
};
```

States do **not** hold transition logic. Transitions are requested through `EngineContext`:

```cpp
void GameplayState::OnUpdate(EngineContext& ctx, float dt)
{
    m_stateMachine.ProcessPending(ctx);
    m_simulation->Update(dt);
    if (m_simulation->IsGameOver())
        ctx.RequestTransition<CreditsState>();
    if (m_pauseRequested)
        ctx.RequestPush<PauseState>();
}
```

### Hybrid Model: Flat Transitions + Push/Pop Stack

Two operations:

- **Flat transition** (replace): active state torn down and replaced. `Splash -> MainMenu -> Gameplay`.
- **Push/pop** (modal stack, unlimited depth): new state pushed on top, suspending states below. `Gameplay` pushes `Pause` pushes `Settings`. Pop returns up the stack.

All transitions are deferred: queued during the frame, processed at the start of the next frame.

### Push/Pop Negotiation

When a state is pushed, both sides negotiate background behaviour. The result is the intersection -- both must agree:

```cpp
// The pushed state declares what it wants
auto PauseState::GetBackgroundPreferences() const -> BackgroundPreferences
{
    return { .WantsBackgroundRender = true, .WantsBackgroundUpdate = false };
}

// The suspended state declares what it allows
auto GameplayState::GetSuspensionPolicy() const -> SuspensionPolicy
{
    return { .AllowBackgroundRender = true, .AllowBackgroundUpdate = false };
}
```

| Pushed wants | Suspended allows | Result |
|---|---|---|
| Render = true | AllowRender = true | Suspended state renders |
| Render = true | AllowRender = false | No render |
| Update = true | AllowUpdate = false | No update |

### State Registration and Transition Validation

```cpp
void Build(AppBuilder& builder) override
{
    builder.AddState<SplashState>({
        .Initial = true,
        .Capabilities = { Capability::Presentation },
    });
    builder.AddState<GameplayState>({
        .Capabilities = { Capability::Simulation, Capability::Rendering, Capability::Presentation },
    });

    builder.AddTransition<SplashState, MainMenuState>();
    builder.AddTransition<MainMenuState, GameplayState>();
}
```

Every `RequestTransition<T>()` must match a declared transition. Undeclared transitions are a startup validation error.

`builder.AllowDynamic<T>()` is an escape hatch for states needing unrestricted transitions (mod support, dynamic flow). Not the default -- the declared graph serves as documentation and catches typos early.

---

## Overlay System

Overlays are lightweight decorations that render on top of the active state. They don't own the frame -- the active state drives update and render; overlays add to it.

**Overlays are not states.** The test: "does this thing need to control whether the state below it keeps running?" If yes, it's a pushed state. If no, it's an overlay.

### Types

All overlays are **persistent**: registered at startup, survive state transitions. Auto-deactivated if capabilities unmet. Activated/deactivated at runtime via `ActivateOverlay()` / `DeactivateOverlay()`.

Game-specific UI (HUD elements, tutorial popups, interaction prompts) is **not** an overlay -- it's a plugin-injected `IStateUI` implementation with state lifetime. Overlays are engine-level decorations with application lifetime; `IStateUI` is game-level UI with state lifetime. They both submit to canvases, but differ in data coupling (overlays access `EngineContext` subsystems; `IStateUI` accesses whatever the state provides) and lifecycle ownership. See the **State UI** section below and [game_framework.md](game_framework.md) for concrete usage.

### Interface and Registration

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

builder.RegisterOverlay<DebugConsoleOverlay>({
    .RequiredCapabilities = {},         // all states
    .DefaultActive = false,             // toggled at runtime
});
```

### Execution Order

Overlay ordering follows **registration order** for both input dispatch and rendering:

- **Input**: top-down (last registered first). Overlays can consume events.
- **Update**: all active overlays, after the active state.
- **Render**: bottom-up (state first, then overlays in registration order).

### Accessing Simulation Data

Overlays use `EngineContext` subsystems, not downcasts:

```cpp
void PerformanceMetricsOverlay::OnRender(EngineContext& ctx)
{
    auto& debug = ctx.GetAppSubsystem<Renderer>().GetDebugCanvas();
    debug.DrawText(FormatFps(ctx.GetAppSubsystem<Time>().GetFps()), {.Position = topLeft});

    if (auto* diagnostics = ctx.TryGetStateSubsystem<SimulationDiagnostics>())
        debug.DrawText(FormatEntityCount(diagnostics->GetEntityCount()), {.Position = belowFps});
}
```

`TryGetStateSubsystem<T>()` returns `nullptr` when no matching subsystem is active. Explicit, null-safe, no coupling to concrete state types.

---

## State UI

States can optionally have a **plugin-injected UI provider** (`IStateUI`). The engine owns the lifecycle wiring (when to forward events, when to update, when to render). The game owns the content (which panels exist, what data they show, what modals they present).

This is opt-in. Simple states (splash screens, loading screens) handle their own rendering directly in `OnRender`. States with interactive, layered UI (gameplay, editor, menus) use `IStateUI` to keep the state implementation reusable and the UI implementation game-specific.

### Interface

```cpp
class IStateUI
{
public:
    virtual ~IStateUI() = default;

    virtual void OnAttach(EngineContext& ctx) {}
    virtual void OnDetach(EngineContext& ctx) {}
    virtual void OnSuspend(EngineContext& ctx) {}
    virtual void OnResume(EngineContext& ctx) {}
    virtual void OnUpdate(EngineContext& ctx, float deltaTime) {}
    virtual void OnRender(EngineContext& ctx) {}
    virtual void OnEvent(EngineContext& ctx, Event& event) {}
    virtual auto GetName() const -> std::string_view = 0;
};
```

The lifecycle mirrors `IOverlay` plus `OnSuspend`/`OnResume` (forwarded when a modal state is pushed/popped over the owning state). All methods receive `EngineContext&` -- implementations look up whatever they need (Renderer for canvas access, subsystems for simulation data, UI toolkit for widget management).

### Registration

Registered per-state via the existing `ForState<T>()` pattern:

```cpp
void JourneyGamePlugin::Build(AppBuilder& builder)
{
    builder.ForState<GameplayState>().RegisterStateUI<JourneyGameUI>();
}
```

### Event Forwarding

The engine state forwards events to `IStateUI` before any game-level input handling. The implementation (or the UI toolkit it delegates to) performs hit testing, focus management, and modal input blocking. Consumed events don't reach game systems:

```cpp
void GameplayState::OnEvent(EngineContext& ctx, Event& event)
{
    if (m_stateUI)
    {
        m_stateUI->OnEvent(ctx, event);
        if (event.Handled)
            return;
    }
    // Unconsumed events: available for InputAction subsystem during Update
}
```

### Lifecycle

| Event | When | Purpose |
|---|---|---|
| `OnAttach(ctx)` | After state enters, IStateUI created | Cache canvas pointers, create widget trees, bind data |
| `OnDetach(ctx)` | Before state exits, IStateUI destroyed | Release widget trees, clean up |
| `OnSuspend(ctx)` | Modal state pushed (e.g. PauseState) | Pause animations, gray out, stop tooltip tracking |
| `OnResume(ctx)` | Modal state popped | Resume animations, restore interactivity |
| `OnUpdate(ctx, dt)` | Each frame, after simulation update | Refresh data bindings, animate UI elements |
| `OnRender(ctx)` | Each frame, after scene submission | Submit draw commands to canvases |
| `OnEvent(ctx, event)` | Before game input handling | UI toolkit hit testing, focus, modal blocking |

### IStateUI is Not an Overlay

The litmus test: overlays are **engine-level decorations** (FPS counter, debug console, render graph visualiser) with **application lifetime**. `IStateUI` is **game-level UI** (health bar, inventory, menus, editor panels) with **state lifetime**. They both submit to canvases, but differ in:

- **Data coupling**: Overlays access `EngineContext` subsystems. `IStateUI` implementations access game-specific data (ECS world, simulation state) through whatever the owning state provides.
- **Lifecycle**: Overlays are persistent and capability-gated. `IStateUI` is created/destroyed with the state.
- **Visibility control**: Overlay visibility is toggled via `ActivateOverlay`/`DeactivateOverlay`. `IStateUI` panel visibility is driven by game state (sub-state machine, ECS singletons, player actions).

### Layering and Input Ordering

Visual z-ordering and input hit testing are handled by the **UI toolkit** (a separate engine subsystem), not by `IStateUI` itself. Implementations create widgets on named layers within the toolkit (`HUD`, `Panel`, `Modal`, `Tooltip`). The toolkit manages z-order for rendering and input dispatch.

@todo Currently one `IStateUI` per state. Designed for future multi-registration where multiple plugins contribute UI to the same state (e.g. core HUD plugin + minimap plugin + quest tracker plugin). In that model, `IStateUI` instances are unordered peers -- the toolkit's layer system handles visual and input ordering. Registration order serves as tiebreaker for non-widget keyboard event dispatch.

@todo Visual UI designer tooling (similar to UMG). The UI toolkit's widget trees should be authorable from a visual editor, with `IStateUI` loading designer-authored layouts and binding them to runtime data.

@todo Tag-gated widget activation. The UI toolkit could support `GameplayTag`-based visibility on individual widgets/panels, similar to Lyra's HUD activation tags. A health bar widget declares `RequiredTags: {"State.Alive"}` and auto-hides when the tag isn't active.

---

## EngineContext

The single service-access mechanism for all states and overlays. No globals. No `Application::Get()`.

```cpp
class EngineContext
{
public:
    // -- Subsystem access --
    template<typename T> auto GetAppSubsystem() const -> T&;
    template<typename T> auto TryGetAppSubsystem() const -> T*;
    template<typename T> auto GetStateSubsystem() const -> T&;
    template<typename T> auto TryGetStateSubsystem() const -> T*;
    template<typename T> auto TryGet() const -> T*;  // state first, then app

    // -- State transitions (deferred) --
    template<std::derived_from<IApplicationState> T> void RequestTransition();
    template<std::derived_from<IApplicationState> T> void RequestPush();
    void RequestPop();

    // -- Overlay operations (deferred) --
    void ActivateOverlay(OverlayId id);
    void DeactivateOverlay(OverlayId id);

    // -- Query --
    auto GetProject() const -> const ProjectDescriptor&;
    auto GetAppDescriptor() const -> const AppDescriptor&;

    void RequestStop();

private:
    SubsystemRegistry m_appSubsystems;
    SubsystemRegistry m_stateSubsystems;
    ApplicationStateMachine* m_stateMachine = nullptr;
};
```

Passed to every lifecycle method by reference. **Reads are direct** (subsystem queries are const, registries don't change after startup). **Writes are deferred** (transitions, overlay toggles queued and processed at frame boundaries). States request transitions through `EngineContext` -- this makes the state interface small and testable (mock the context, not a state machine).

---

## Application

```cpp
class Application
{
public:
    struct CommandLineArgs { int Count; char** Args; };

    explicit Application(const CommandLineArgs& args);
    ~Application();

    template<std::derived_from<IPlugin> T, typename... TArgs>
    void AddPlugin(TArgs&&... args);

    void Run();

private:
    auto Initialise() -> Result<void>;
    void Loop();
    void Shutdown();
    void OnEvent(Event& event);

    CommandLineArgs m_args{};
    EngineContext m_context;
    ApplicationStateMachine m_stateMachine;
    OverlayStack m_overlayStack;
    EventQueue m_eventQueue;
    std::vector<std::unique_ptr<IPlugin>> m_plugins;
    bool m_running = false;
};
```

Application does **not** own or know about `Simulation`, `Scene`, flecs, or any rendering calls beyond coordinating `OnRender` on the active state and overlays.

### Main Loop

```cpp
void Application::Loop()
{
    while (m_running)
    {
        auto& time = m_context.GetAppSubsystem<Time>();
        auto& renderer = m_context.GetAppSubsystem<Renderer>();

        time.BeginFrame();

        // 1. Process deferred state/overlay operations
        m_stateMachine.ProcessPending(m_context);
        m_overlayStack.ProcessPending(m_context);

        // 2. Drain input: overlays first (top-down, can consume), then active state
        m_eventQueue.Drain([&](Event& e) {
            m_overlayStack.DispatchEvent(m_context, e);
            if (not e.Handled)
                m_stateMachine.DispatchEvent(m_context, e);
        });

        // 3. Update: active state, then overlays
        const float dt = time.GetDeltaTime();
        m_stateMachine.Update(m_context, dt);
        m_overlayStack.Update(m_context, dt);

        // 4. Render: state first, overlays on top
        renderer.BeginFrame();
        m_stateMachine.Render(m_context);
        m_overlayStack.Render(m_context);
        renderer.EndFrame();

        time.EndFrame();
    }
}
```

---

## Subsystems

Subsystems are **scoped services** with managed lifetimes. They provide capabilities (physics, audio, rendering) that states, overlays, and ECS systems consume. A subsystem is a service with behaviour and an API -- data belongs in ECS components, assets, or config structs.

### Two Scopes

| Scope | Base class | Lifetime | Examples |
|---|---|---|---|
| Application | `AppSubsystem` | App start to stop | Renderer, Window, Input, Time, AssetService, TagRegistry |
| State | `StateSubsystem` | State enter to exit | PhysicsSubsystem, AudioSubsystem, NavigationSubsystem |

The base class determines scope. Concept-constrained overloads on `AppBuilder` route registration automatically.

### Abstract-Type Resolution

Consumers access subsystems by abstract interface (`ctx.GetAppSubsystem<Window>()`), not concrete type. The abstract interface is specified as a second template parameter during registration:

```cpp
// Registers under both typeid(SDL3Window) and typeid(Window).
// When no second parameter is given, only the concrete type is registered.
builder.RegisterSubsystem<SDL3Window, Window>({...});
```

Duplicate abstract registrations (two plugins both registering a `Window`) are a startup error.

### Dependency Declaration

Subsystems declare dependencies at registration time via `DependsOn`, enabling topological sort before `Initialise()` is called. `Initialise()` returns `Result<void>` for runtime validation:

```cpp
builder.RegisterSubsystem<PhysicsSubsystem>({
    .RequiredCapabilities = { Capability::Simulation },
    .DependsOn = { typeid(AssetService) },
});
```

```cpp
class PhysicsSubsystem : public StateSubsystem
{
public:
    auto Initialise(SubsystemRegistry& registry) -> Result<void> override
    {
        auto* assets = registry.Try<AssetService>();
        if (not assets)
            return std::unexpected(SubsystemError::MissingDependency("AssetService"));
        m_assetManager = assets;
        return {};
    }

    void Shutdown() override { /* release resources */ }
};
```

Dependencies enable topological ordering: init follows dependency order, shutdown follows reverse order. If `Initialise` fails, the state transition is aborted and the error is surfaced.

### Push/Pop and State Subsystem Lifecycle

State-scoped subsystems are owned by `EngineContext`. Their lifecycle is tied to **flat transitions** only:

- **Flat transition** (replace): old state subsystems shut down, new state subsystems created.
- **Push/pop**: state subsystems **persist**. Pushed states run with the same subsystem set as the suspended state below them.

This means `PauseState` (pushed over `GameplayState`) can still access `PhysicsSubsystem` if needed, and physics world state is preserved.

### Static Accessor for ECS Systems

flecs system callbacks are lambdas with flecs-controlled signatures -- they cannot receive injected references. `StateSubsystems` provides a bounded static accessor:

```cpp
world.system<Transform, RigidBody>()
    .each([](Transform& t, RigidBody& rb) {
        auto& physics = StateSubsystems::Get<PhysicsSubsystem>();
        physics.SyncTransform(t, rb);
    });
```

Bound on state enter, unbound on state exit. Lambda capture is the preferred alternative when practical. This is the correct permanent mechanism for the flecs callback constraint, not a migration target.

### Subsystems vs ECS World Singletons

| | Subsystems | World Singletons |
|---|---|---|
| Nature | Services with behaviour and APIs | Data that ECS systems read/write |
| Examples | PhysicsSubsystem, AudioSubsystem | ActiveGameState, WeatherData |
| Access | `StateSubsystems::Get<T>()` or `ctx.GetStateSubsystem<T>()` | `world.get<T>()` or queried in system signatures |

---

## Plugins

Plugins are the **sole unit of extension**. `Application::AddPlugin<T>()` is the only public API for composition.

### Interface

```cpp
class IPlugin
{
public:
    virtual ~IPlugin() = default;
    virtual void Build(AppBuilder& builder) = 0;
    virtual auto GetName() const -> std::string_view = 0;
};
```

No `OnStartup()` or `OnShutdown()`. Plugins register interest in specific lifecycle events during `Build()`:

```cpp
void Build(AppBuilder& builder) override
{
    builder.DependsOn<CorePlugin>();

    builder.RegisterSubsystem<PhysicsSubsystem>({
        .RequiredCapabilities = { Capability::Simulation },
        .Config = PhysicsConfig{ .Gravity = {0.f, -9.81f, 0.f} },
    });

    builder.OnStateEnter<GameplayState>([](EngineContext& ctx) {
        auto& physics = ctx.GetStateSubsystem<PhysicsSubsystem>();
        physics.LoadCollisionData("physics/world.col");
    });

    builder.OnAppReady([](EngineContext& ctx) { /* cross-plugin wiring */ });
}
```

Plugin dependencies are validated and `Build()` calls are ordered. Missing dependencies produce a startup error. Circular dependencies are rejected.

### What Plugins Register

| Registration | Method |
|---|---|
| Application states | `AddState<T>()` |
| State transitions | `AddTransition<A, B>()` |
| Overlays | `RegisterOverlay<T>()` |
| App/state subsystems | `RegisterSubsystem<T>()` |
| Render features | `RegisterRenderFeature<T>()` |
| ECS systems/components/globals | `RegisterSystem()`, `RegisterComponent()`, `RegisterGlobal()` |
| Sub-states | `ForState<T>().RegisterSubState()` |
| State UI | `ForState<T>().RegisterStateUI<U>()` |
| Gameplay tags | `RegisterTag()` / `RegisterTagFile()` |
| App capabilities | `ProvideCapability()` |
| Lifecycle hooks | `OnAppReady()`, `OnStateEnter<T>()`, etc. |

### AppBuilder: Typed Registrar Store

`AppBuilder` is internally a type-keyed container of registrars -- domain-specific objects that accumulate and validate registrations. Convenience methods (`AddState`, `RegisterSubsystem`) are thin wrappers:

```cpp
// Equivalent:
builder.AddState<GameplayState>({...});
builder.Registrar<StateRegistrar>().Register<GameplayState>({...});
```

Registrars encapsulate domain logic: `SystemRegistrar` does topological sort, `StateRegistrar` validates exactly one initial state, `TagRegistrar` merges tag files. Plugins can define custom registrar types (e.g. `ReplicationRegistrar`) without modifying `AppBuilder`:

```cpp
builder.Registrar<ReplicationRegistrar>().Register({
    .ComponentType = typeid(Transform),
    .Policy = ReplicationPolicy::Interpolated,
});
```

After all plugins call `Build()`, `AppBuilder::Finalise()` produces a read-only `AppDescriptor`. Consumers query by registrar type; `TryGet` returns `nullptr` if no plugin registered that type.

### Backend Selection via Plugin Composition

```cpp
// Game
app.AddPlugin<VulkanRendererPlugin>();   // VulkanRenderer : Renderer : AppSubsystem
app.AddPlugin<SDLWindowPlugin>();        // SDLWindow : Window : AppSubsystem
app.AddPlugin<JourneyGamePlugin>();
app.Run();

// Headless tool
app.AddPlugin<NullRendererPlugin>();
app.AddPlugin<HeadlessWindowPlugin>();
app.Run();
```

The plugin set IS the application's capability manifest. Plugin group types compose sets:

```cpp
struct JourneyDefaultPlugins : IPlugin
{
    void Build(AppBuilder& builder) override
    {
        builder.AddPlugin<VulkanRendererPlugin>();
        builder.AddPlugin<SDLWindowPlugin>();
        builder.AddPlugin<JourneyContentPlugin>();
        builder.AddPlugin<SplashPlugin>();
    }
    auto GetName() const -> std::string_view override { return "JourneyDefaults"; }
};
```

---

## Configuration

### Problem

The monolithic `EngineConfig` bundles all subsystem configs, grows with every feature, and loads config for modules that may not be present.

### Solution: Plugin-Driven Config

Each plugin owns its config types. `AppBuilder` provides a cached loader:

```cpp
void PhysicsPlugin::Build(AppBuilder& builder) override
{
    auto config = builder.LoadConfig<PhysicsConfig>("physics");
    builder.RegisterSubsystem<PhysicsSubsystem>({
        .RequiredCapabilities = { Capability::Simulation },
        .Config = std::move(config),
    });
}
```

- Config files live in the project's config directory (e.g. `config/physics.toml`).
- Multiple plugins reading the same file parse once (cached).
- If a plugin isn't loaded, its config is never read.
- @todo: hot-reload via file watcher + `OnConfigReloaded(EngineContext&)` callback. Stub the callback interface now; implement the watcher later.

---

## Rendering Integration

### Principle

`EngineRuntime` is decomposed into app-scoped subsystems. The `Renderer` doesn't know about scenes. States and overlays submit to typed canvases. The render graph is internal.

### Decomposed Services

| Old (EngineRuntime) | Becomes | Registered by |
|---|---|---|
| Window | `AppSubsystem` | `SDLWindowPlugin` |
| Input | `AppSubsystem` | `SDLInputPlugin` |
| Time | `AppSubsystem` | Core engine plugin |
| Renderer | `AppSubsystem` | `VulkanRendererPlugin` |
| BlendableEffectRegistry | `AppSubsystem` (standalone) | Rendering plugin |

### Canvases

Canvases are **typed, per-frame data collectors** that accumulate submission data from game code. RenderFeatures **declare** the canvas type they consume; the Renderer creates canvases based on feature registrations. One canvas per type.

| Canvas type | Declared by | Used by |
|---|---|---|
| `SceneCanvas` | `SceneFeature` | GameplayState, EditorState |
| `UICanvas` | `UIFeature` | Any state (splash, menu, HUD), IStateUI |
| `DebugCanvas` | `DebugFeature` | Debug overlays, editor gizmos |

```cpp
builder.RegisterRenderFeature<SceneFeature>({
    .CanvasType = typeid(SceneCanvas),
    .RequiredCapabilities = { Capability::Rendering },
});
builder.RegisterRenderFeature<UIFeature>({
    .CanvasType = typeid(UICanvas),
    // No capabilities -- always active. Splash, menu, and gameplay all need UI.
});
// Features without canvases (read from render graph, not game code):
builder.RegisterRenderFeature<SSAOFeature>({
    .RequiredCapabilities = { Capability::Rendering },
});
```

States and overlays access canvases by type. **Cache pointers in `OnEnter` for hot-path access:**

```cpp
// O(1) type-indexed access:
auto& scene = renderer.GetCanvas<SceneCanvas>();

// For hot paths, cache during OnEnter:
m_sceneCanvas = &renderer.GetCanvas<SceneCanvas>();
```

Canvases accept **multiple submissions** with parameters for split-screen, editor viewports, or picture-in-picture:

```cpp
renderer.GetCanvas<SceneCanvas>().Submit(*scene, { .Camera = cam, .Viewport = rect });
renderer.GetCanvas<SceneCanvas>().Submit(*scene, { .Camera = editorCam, .Target = offscreen });
```

States and overlays never touch `RenderDevice` or `RenderGraph`. They submit to canvases.

### Render Feature Lifecycle

Features are owned by the `Renderer`, activated/deactivated by capability matching on state transitions. Created once at startup (GPU resources persist), just enabled/disabled per state:

```cpp
builder.RegisterRenderFeature<SSAOFeature>({
    .RequiredCapabilities = { Capability::Rendering },
    .Phase = RenderPhase::PostOpaque,
    .Order = 0,
    .Config = SSAOConfig{ .Radius = 0.5f, .Samples = 16 },
});
```

Always-on features (composition pass, present pass) use empty `RequiredCapabilities`.

### Data Flow

```
renderer.BeginFrame()           clear canvases
  |
GameplayState::OnRender()       submit to SceneCanvas, UICanvas
Overlays::OnRender()            submit to DebugCanvas, UICanvas
  |
renderer.EndFrame()
  RenderOrchestrator::BuildGraph()
    scene features -> AddPasses (opaque, post-process, composite)
    UI feature -> AddPasses (screen-space)
    debug feature -> AddPasses (debug text, gizmos)
    present feature -> AddPasses (swapchain blit)
  RenderGraph::Compile()        topological sort, dead-pass cull
  RenderGraph::Execute()        GPU work
  present
```

For hot-path performance goals (visibility, extraction, instancing), see [rendering_performance.md](rendering_performance.md).

---

## Lifecycle Timeline

The single authoritative sequence for all lifecycle events.

### Startup

```
Application::Run()
  |
  1. Load ProjectDescriptor
  2. Validate plugin dependency graph
  3. Plugin::Build(builder) on each plugin (dependency order)
  4. Validate state transition graph and subsystem dependency graph
  5. Construct and initialise app-scoped subsystems (dependency order)
     - Each Initialise() returns Result<void>; failure aborts startup
  6. Fire OnAppReady callbacks
  7. Construct and initialise state-scoped subsystems for initial state (per capabilities)
  8. Activate render features and overlays for initial state (per capabilities)
  9. Initial state OnEnter(ctx) -> Result<void>
     - Failure: error bubbles to Application. Can transition to a fallback state
       (error screen) or abort gracefully with diagnostics.
```

### Steady-State Frame

```
time.BeginFrame()
  |
  1. ProcessPending: deferred transitions, push/pop
     |  On transition:
     |    a. Active state OnExit()
     |    b. State subsystems shut down (reverse dependency order)
     |    c. StateSubsystems accessor unbound
     |    d. Render features deactivated for old capabilities
     |    e. New state subsystems constructed + initialised (dependency order)
     |    f. StateSubsystems accessor rebound
     |    g. Render features + overlays activated for new capabilities
     |    h. New state OnEnter() -> Result<void>
  |
  2. ProcessPending: activate/deactivate overlays
  |
  3. Drain events: overlays top-down (can consume), then active state
  |
  4. Update: active state (+suspended if negotiated), then all active overlays
  |
  5. Render: active state (+suspended if negotiated), then overlays (bottom-up)
     wrapped in renderer.BeginFrame() / EndFrame()
  |
time.EndFrame()
```

### Shutdown

```
m_running = false
  |
  1. Active state OnExit()
  2. State subsystems shut down (reverse dependency order)
  3. StateSubsystems accessor unbound
  4. Overlays detached
  5. App subsystems shut down (reverse dependency order)
  6. Plugins destroyed (reverse registration order)
```

---

## Threading and Parallelism

### Current Model: Single-Threaded

The application loop, state machine, and subsystem lifecycle are single-threaded. Correctness first; the architecture is designed so parallelism can be added incrementally.

### Where Parallelism Will Be Introduced

| Area | Approach | Priority |
|---|---|---|
| Render extraction | Dedicated thread/pool, double-buffered frame arenas | High |
| ECS systems | flecs built-in multithreading (pipeline stages, worker threads) | High |
| Asset loading | Async I/O with engine awaitables (`co_await LoadAsset("path")`) | Medium |
| Physics | Jolt's own job system, runs within ECS update phase | Low |

### Design Constraints for Thread Safety

1. **`EngineContext` read paths are const.** Subsystem registries don't change after startup. Safe from any thread.
2. **State transitions are deferred.** `RequestTransition<T>()` queues; drained on the main thread at frame start.
3. **Frame data is isolated.** Extraction and submission use separate per-frame arenas.
4. **Subsystem lifecycle is main-thread only.** All construction/destruction at frame boundaries.
5. **Event dispatch is main-thread only.** Batched, drained on the main thread.

---

## Design Rationale

Decisions that aren't self-evident from reading the architecture above.

| Decision | Rationale |
|---|---|
| Static accessor (`StateSubsystems`) is permanent | flecs callbacks have flecs-controlled signatures; they cannot receive injected references. Lambda capture is preferred when practical. The accessor is bound/unbound on state enter/exit. |
| Push/pop authority is an intersection | Neither the pushed state nor the suspended state has unilateral control. The intersection prevents maintenance hazards where every `PushState` caller must know the correct config. |
| `Simulation` uses `ServiceProvider` concept, not `EngineContext` | Keeps `Simulation` framework-agnostic. Headless tools and tests provide a `StandaloneServiceProvider`. No coupling, no manual bridging. |
| Sub-state machines are member variables, not subsystems | State machines are internal orchestration, not shared services. Each `IApplicationState` creates its own `StateMachine<InternedString>` from plugin-registered descriptors. |
| State subsystems owned by EngineContext, not Simulation | Application manages lifecycle on flat transitions. `Simulation` accesses services via `ServiceProvider`. Subsystems persist through push/pop. |
| AppBuilder uses typed registrar store | Plugins can define custom registrar types (e.g. `ReplicationRegistrar`) without modifying `AppBuilder`. Each registrar encapsulates its own domain logic. |
| `BlendableEffectRegistry` is a separate `AppSubsystem` | Game volumes push effects, renderer reads them. Standalone subsystem makes this relationship explicit and testable. |
| Editor panels are not overlays | Panels need docking, persistence, serialisation -- fundamentally different from the overlay stack. EditorPanelManager implements `IStateUI` -- same lifecycle contract, different internal complexity. |
| `IStateUI` is not an overlay | Overlays have application lifetime and are capability-gated. `IStateUI` has state lifetime and is visibility-driven by game state. Both submit to canvases. |
| One `IStateUI` per state (for now) | Avoids ordering/priority questions between multiple UI providers. The UI toolkit's layer system handles visual ordering. Multi-registration is a planned expansion. |
| Transitions validated at startup | Declared transition graph catches typos early and serves as documentation. `AllowDynamic<T>()` escape hatch for dynamic flow. Applies to both app-level and sub-state transitions. |
| No overlay-scoped subsystems | Overlays are lightweight and shouldn't own services. |
| No transient overlays | Every real-world case (toasts, popups, tutorials) is either state-managed UI or a persistent overlay with runtime activation. Transient overlays add API complexity with no concrete use case. |
| Canvases are one-per-type, feature-declared | RenderFeatures declare their canvas type. Renderer creates them. Type-indexed access (no string lookups). Multi-view via submission parameters, not multiple canvas instances. |

---

## Ownership Summary

| Concept | Owner | Accessed via |
|---|---|---|
| Platform subsystems | EngineContext (app registry) | `ctx.GetAppSubsystem<T>()` |
| State subsystems | EngineContext (state registry) | `ctx.GetStateSubsystem<T>()` or `StateSubsystems::Get<T>()` |
| Application state machine | Application | `ctx.RequestTransition<T>()` |
| Sub-state machines | IApplicationState (member variable) | State's own `m_stateMachine` |
| Overlay stack | Application | `ctx.ActivateOverlay()` / `ctx.DeactivateOverlay()` |
| Event queue | Application | Dispatched by Application each frame |
| Plugins | Application (permanent) | `Build()` called once |
| State UI | IApplicationState (per-state, optional) | Created by state from plugin descriptor, `IStateUI` interface |
| ECS world, scenes | Simulation | See [game_framework.md](game_framework.md) |