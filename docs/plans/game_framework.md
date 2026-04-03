# Game Framework

**Status:** Planned
**Parent document:** [application_architecture_v2.md](application_architecture_v2.md)
**Last updated:** 2026-04-03

How `Simulation` works as a standalone simulation engine, how `GameplayState` and `EditorState` adapt it into the application lifecycle, how per-state sub-state machines work, the service provider pattern, and the `IStateUI` pattern for game-specific UI.

---

## Simulation: Standalone Simulation Engine

`Simulation` (previously `Game`) owns the flecs world and scene management. It is usable without `Application`, states, overlays, or a window -- headless tests, CLI tools, and the editor all use `Simulation` directly.

`Simulation` does **not** own state machines or state-scoped subsystems. State machines are member variables of `IApplicationState` implementations. State-scoped subsystems are owned by `EngineContext`. `Simulation` accesses external services via a concept:

```cpp
template<typename T>
concept ServiceProvider = requires(T provider) {
    { provider.template Get<AssetService>() } -> std::same_as<AssetService&>;
    { provider.template TryGet<AssetService>() } -> std::same_as<AssetService*>;
};
```

```cpp
class Simulation
{
public:
    explicit Simulation(const AppDescriptor& descriptor);

    auto Initialise(ServiceProvider auto& services) -> Result<void>;
    void Update(float deltaTime);
    void Shutdown();

    auto GetWorld() -> flecs::world&;
    auto GetCurrentScene() -> Scene*;

    void LoadScene(std::string_view scenePath);

private:
    flecs::world m_world;
    std::unique_ptr<Scene> m_currentScene;
    const AppDescriptor& m_descriptor;
};
```

`Simulation::Initialise` pulls what it needs from whatever service provider is given:

```cpp
auto Simulation::Initialise(ServiceProvider auto& services) -> Result<void>
{
    if (auto* assets = services.template TryGet<AssetService>())
        m_assetService = assets;

    if (auto* tags = services.template TryGet<GameplayTagRegistry>())
        m_tagRegistry = tags;

    return {};
}
```

---

## Service Provider Adapters

### EngineContextServiceProvider (used by GameplayState)

```cpp
struct EngineContextServiceProvider
{
    EngineContext& Ctx;

    template<typename T>
    auto Get() -> T& { return Ctx.GetAppSubsystem<T>(); }

    template<typename T>
    auto TryGet() -> T* { return Ctx.TryGetAppSubsystem<T>(); }
};
```

### StandaloneServiceProvider (used by headless tools and tests)

Registry-based, type-erased container. Scales without modifying the provider when new service types are added:

```cpp
class StandaloneServiceProvider
{
public:
    template<typename T>
    void Register(T& service) { m_services[typeid(T)] = &service; }

    template<typename T>
    auto TryGet() -> T*
    {
        auto it = m_services.find(typeid(T));
        return it != m_services.end() ? static_cast<T*>(it->second) : nullptr;
    }

    template<typename T>
    auto Get() -> T&
    {
        auto* ptr = TryGet<T>();
        assert(ptr and "Service not registered");
        return *ptr;
    }

private:
    std::unordered_map<std::type_index, void*> m_services;
};
```

### Headless usage

```cpp
auto project = ProjectDescriptor::Discover();
auto config = EngineConfig::Load(project);

AppBuilder builder(project, config);
JourneyContentPlugin plugin;
plugin.Build(builder);
auto descriptor = builder.Finalise();

StandaloneServiceProvider services;
services.Register(assetService);
services.Register(tagRegistry);

Simulation simulation(descriptor);
simulation.Initialise(services);
simulation.Update(1.0f / 60.0f);
```

---

## GameplayState: The Application Adapter

Wraps `Simulation` into the `IApplicationState` lifecycle. Owns a `StateMachine<InternedString>` for sub-state management (Playing, Loading, Cutscene). Game-specific UI is injected via `IStateUI` (see [application_architecture_v2.md](application_architecture_v2.md#state-ui) for the interface definition):

```cpp
class GameplayState : public IApplicationState
{
public:
    auto OnEnter(EngineContext& ctx) -> Result<void> override
    {
        m_simulation = std::make_unique<Simulation>(ctx.GetAppDescriptor());
        EngineContextServiceProvider services{ .Ctx = ctx };
        auto result = m_simulation->Initialise(services);
        if (not result)
            return std::unexpected(result.error());

        // Build sub-state machine from plugin-registered descriptors
        auto descriptors = ctx.GetAppDescriptor().GetSubStates<GameplayState>();
        for (const auto& desc : descriptors)
            m_stateMachine.RegisterState(desc.Name, desc.Callbacks);
        m_stateMachine.SetInitial(descriptors.GetInitial());

        // Wire ECS sync: update ActiveGameState singleton on every sub-state transition
        m_stateMachine.OnTransition([this](InternedString from, InternedString to) {
            m_simulation->GetWorld().set<ActiveGameState>({ .Current = to, .Previous = from });
        });

        m_stateMachine.Start(ctx);

        m_sceneCanvas = &ctx.GetAppSubsystem<Renderer>().GetCanvas<SceneCanvas>();

        // Create plugin-registered IStateUI (if any)
        if (auto factory = ctx.GetAppDescriptor().GetStateUIFactory<GameplayState>())
        {
            m_stateUI = factory();
            m_stateUI->OnAttach(ctx);
        }

        return {};
    }

    void OnExit(EngineContext& ctx) override
    {
        if (m_stateUI)
            m_stateUI->OnDetach(ctx);
        m_stateUI.reset();
        m_simulation->Shutdown();
        m_simulation.reset();
    }

    void OnSuspend(EngineContext& ctx) override
    {
        if (m_stateUI)
            m_stateUI->OnSuspend(ctx);
    }

    void OnResume(EngineContext& ctx) override
    {
        if (m_stateUI)
            m_stateUI->OnResume(ctx);
    }

    void OnEvent(EngineContext& ctx, Event& event) override
    {
        // State UI gets input first (toolkit hit testing, focus, modals)
        if (m_stateUI)
        {
            m_stateUI->OnEvent(ctx, event);
            if (event.Handled)
                return;
        }
        // Unconsumed events: available for InputAction subsystem during Update
    }

    void OnUpdate(EngineContext& ctx, float dt) override
    {
        m_stateMachine.ProcessPending(ctx);
        m_simulation->Update(dt);

        if (m_stateUI)
            m_stateUI->OnUpdate(ctx, dt);

        if (m_simulation->IsGameOver())
            ctx.RequestTransition<CreditsState>();
        if (m_pauseRequested)
            ctx.RequestPush<PauseState>();
    }

    void OnRender(EngineContext& ctx) override
    {
        if (const auto* scene = m_simulation->GetCurrentScene())
            m_sceneCanvas->Submit(*scene);

        if (m_stateUI)
            m_stateUI->OnRender(ctx);
    }

    auto GetSuspensionPolicy() const -> SuspensionPolicy override
    {
        return {
            .AllowBackgroundRender = true,
            .AllowBackgroundUpdate = false,
        };
    }

    auto GetName() const -> std::string_view override { return "GameplayState"; }

private:
    std::unique_ptr<Simulation> m_simulation;
    StateMachine<InternedString> m_stateMachine;
    std::unique_ptr<IStateUI> m_stateUI;
    SceneCanvas* m_sceneCanvas = nullptr;
};
```

---

## Per-ApplicationState Sub-State Machines

Sub-states (Playing, Loading, Cutscene) are **per-ApplicationState** -- not a game-level concept. Any `IApplicationState` can own a `StateMachine<InternedString>`. Sub-states are registered by plugins via `builder.ForState<T>()` and stored in `AppDescriptor`.

### Plugin Registration

Plugins declare sub-state structure and optional `EngineContext&` callbacks. Free functions, classes, and lambdas all work:

```cpp
// Free functions for testability
void OnEnterPlaying(EngineContext& ctx)
{
    ctx.GetStateSubsystem<AudioSubsystem>().PlayMusic("overworld_theme");
    ctx.GetStateSubsystem<PhysicsSubsystem>().Resume();
}

void OnExitPlaying(EngineContext& ctx)
{
    ctx.GetStateSubsystem<PhysicsSubsystem>().Pause();
}

void JourneyGamePlugin::Build(AppBuilder& builder)
{
    // Sub-states for GameplayState
    builder.ForState<GameplayState>().RegisterSubState({
        .Name = "Loading",
        .OnEnter = [](EngineContext& ctx) {
            ctx.GetStateSubsystem<AudioSubsystem>().FadeOut(0.5f);
        },
    });
    builder.ForState<GameplayState>().RegisterSubState({
        .Name = "Playing",
        .Initial = true,
        .OnEnter = OnEnterPlaying,
        .OnExit = OnExitPlaying,
    });
    builder.ForState<GameplayState>().RegisterSubState({ .Name = "Cutscene" });

    // Sub-state transitions validated at startup:
    builder.ForState<GameplayState>().AddSubTransition("Loading", "Playing");
    builder.ForState<GameplayState>().AddSubTransition("Playing", "Cutscene");
    builder.ForState<GameplayState>().AddSubTransition("Cutscene", "Playing");

    // ECS systems use RunConditions (unchanged):
    builder.RegisterSystem({
        .Name = "MovementSystem",
        .RunCondition = InState("Playing"),
    });
}
```

### Not every state needs sub-states

```cpp
// SplashScreenState has no sub-states -- internal animation only
void SplashPlugin::Build(AppBuilder& builder)
{
    builder.AddState<SplashScreenState>({
        .Initial = true,
        .Capabilities = { Capability::Presentation },
    });
    builder.AddTransition<SplashScreenState, MainMenuState>();
}
```

### Generic StateMachine Template

`StateMachine<TStateId>` is a generic, flat state machine with transition callbacks. Sub-state machines use `InternedString` as the state ID. `ApplicationStateMachine` is a separate, more complex type (push/pop, typed C++ state objects, rich lifecycle) -- they do not share a base class.

```cpp
template<typename TStateId>
class StateMachine
{
public:
    using TransitionCallback = std::function<void(TStateId from, TStateId to)>;

    void RegisterState(TStateId id, StateCallbacks<TStateId> callbacks);
    void TransitionTo(TStateId id);           // deferred
    void OnTransition(TransitionCallback cb); // register observer
    auto GetCurrentState() const -> TStateId;
    auto GetPreviousState() const -> TStateId;
    void ProcessPending(EngineContext& ctx);   // fires callbacks, then observers
    void Start(EngineContext& ctx);            // enter initial state

private:
    TStateId m_current;
    TStateId m_previous;
    std::optional<TStateId> m_pendingTransition;
    std::vector<TransitionCallback> m_observers;
};
```

@note: The ECS integration (updating `ActiveGameState` singleton) is handled entirely by the `OnTransition` callback wired in `GameplayState::OnEnter`. If this callback pattern becomes repetitive across multiple states, it may be upgraded to a thin `EcsStateMachine` wrapper. For now, the callback approach avoids premature abstraction.

### ActiveGameState and RunConditions

`ActiveGameState` is an ECS singleton written by the `OnTransition` callback regardless of which `IApplicationState` owns the state machine:

```cpp
struct ActiveGameState
{
    InternedString Current;
    InternedString Previous;
};
```

`RunCondition` helpers read it (unchanged from current design):

```cpp
RunCondition InState(std::string_view name);
RunCondition NotInState(std::string_view name);
RunCondition HasTag(GameplayTag tag);
```

---

## Editor Architecture

### EditorState

The editor is an application state, not an overlay. It **inverts** the hierarchy: `EditorState` contains the simulation as a viewport rather than overlaying on top of it.

```
GameplayState:   Simulation renders to swapchain directly
EditorState:     Simulation renders to offscreen target; editor composites it into a viewport
```

`EditorState` wraps the same `Simulation` class as `GameplayState`. Its `IStateUI` is `EditorPanelManager`, which handles docking, panel registration, and layout persistence internally:

```cpp
class EditorState : public IApplicationState
{
public:
    auto OnEnter(EngineContext& ctx) -> Result<void> override
    {
        m_simulation = std::make_unique<Simulation>(ctx.GetAppDescriptor());
        EngineContextServiceProvider services{ .Ctx = ctx };
        auto result = m_simulation->Initialise(services);
        if (not result)
            return std::unexpected(result.error());

        // Editor has its own sub-state machine (Editing, PlayMode, PauseMode)
        auto descriptors = ctx.GetAppDescriptor().GetSubStates<EditorState>();
        for (const auto& desc : descriptors)
            m_stateMachine.RegisterState(desc.Name, desc.Callbacks);
        m_stateMachine.SetInitial(descriptors.GetInitial());
        m_stateMachine.Start(ctx);

        m_sceneCanvas = &ctx.GetAppSubsystem<Renderer>().GetCanvas<SceneCanvas>();

        // Create plugin-registered IStateUI (EditorPanelManager)
        if (auto factory = ctx.GetAppDescriptor().GetStateUIFactory<EditorState>())
        {
            m_stateUI = factory();
            m_stateUI->OnAttach(ctx);
        }

        return {};
    }

    void OnExit(EngineContext& ctx) override
    {
        if (m_stateUI)
            m_stateUI->OnDetach(ctx);
        m_stateUI.reset();
        m_simulation->Shutdown();
        m_simulation.reset();
    }

    void OnEvent(EngineContext& ctx, Event& event) override
    {
        if (m_stateUI)
        {
            m_stateUI->OnEvent(ctx, event);
            if (event.Handled)
                return;
        }
        // Editor gizmo interaction, viewport camera, etc.
    }

    void OnUpdate(EngineContext& ctx, float dt) override
    {
        m_stateMachine.ProcessPending(ctx);
        if (m_playMode == PlayMode::Playing)
            m_simulation->Update(dt);

        if (m_stateUI)
            m_stateUI->OnUpdate(ctx, dt);
    }

    void OnRender(EngineContext& ctx) override
    {
        if (const auto* scene = m_simulation->GetCurrentScene())
            m_sceneCanvas->Submit(*scene, { .Target = m_viewportTarget });

        if (m_stateUI)
            m_stateUI->OnRender(ctx);
    }

private:
    std::unique_ptr<Simulation> m_simulation;
    StateMachine<InternedString> m_stateMachine;
    std::unique_ptr<IStateUI> m_stateUI;
    SceneCanvas* m_sceneCanvas = nullptr;
    PlayMode m_playMode = PlayMode::Paused;
};
```

### EditorPanelManager

A concrete `IStateUI` implementation registered by `EditorPlugin`. Handles docking, panel registration, and layout persistence internally. Editor panels are **not** overlays -- they need docking, persistence, and serialisation. Game overlays (FPS counter, debug console) render on top of everything, including the editor.

```cpp
class EditorPanelManager : public IStateUI
{
public:
    void OnAttach(EngineContext& ctx) override
    {
        m_uiCanvas = &ctx.GetAppSubsystem<Renderer>().GetCanvas<UICanvas>();
        // Restore serialised layout, create panel widget trees
    }

    void OnDetach(EngineContext& ctx) override
    {
        // Save layout to disk
    }

    void OnEvent(EngineContext& ctx, Event& event) override
    {
        // Toolkit dispatches to docked panels, handles panel focus/resize
    }

    void OnUpdate(EngineContext& ctx, float dt) override
    {
        // Update property inspectors, scene hierarchy, etc.
    }

    void OnRender(EngineContext& ctx) override
    {
        // Submit panel contents to UICanvas
    }

    auto GetName() const -> std::string_view override { return "EditorPanelManager"; }

private:
    UICanvas* m_uiCanvas = nullptr;
    // Docking state, panel registry, serialised layout...
};
```

Registered by the editor plugin:

```cpp
void EditorPlugin::Build(AppBuilder& builder)
{
    builder.ForState<EditorState>().RegisterStateUI<EditorPanelManager>();
}
```

### Same Architecture, Different Plugins

```cpp
// Game main()
app.AddPlugin<VulkanRendererPlugin>();
app.AddPlugin<SDLWindowPlugin>();
app.AddPlugin<JourneyContentPlugin>();
app.AddPlugin<SplashPlugin>();
app.Run();

// Editor main()
app.AddPlugin<VulkanRendererPlugin>();
app.AddPlugin<SDLWindowPlugin>();
app.AddPlugin<JourneyContentPlugin>();
app.AddPlugin<EditorPlugin>();
app.Run();
```

Same `Application` type, same engine, different plugin sets. `Simulation` is framework-agnostic -- both `GameplayState` and `EditorState` wrap it. A future separate editor executable can also wrap `Simulation` directly without changing this design.

The UI toolkit is a shared engine service (app-scoped subsystem) used by game UI, menus, and editor panels alike. `IStateUI` implementations use the toolkit to create widget trees, handle input, and manage layout. The toolkit is a separate design (out of scope here) -- `IStateUI` is the injection point.

---

## IStateUI: Game-Specific UI

`IStateUI` is defined in [application_architecture_v2.md](application_architecture_v2.md#state-ui). This section shows concrete implementations.

### How game UI works

The game implements `IStateUI` to provide game-specific UI for a state. The engine creates the instance during state entry and wires it into the event/update/render lifecycle automatically. The implementation uses `EngineContext` to access whatever it needs -- the UI toolkit for widget trees, the Renderer for canvas access, subsystems for simulation data.

ECS systems produce data (singletons, components). The `IStateUI` implementation reads that data and draws it:

```cpp
class JourneyGameUI : public IStateUI
{
public:
    void OnAttach(EngineContext& ctx) override
    {
        m_uiCanvas = &ctx.GetAppSubsystem<Renderer>().GetCanvas<UICanvas>();
        auto* sim = ctx.TryGetStateSubsystem<SimulationSubsystem>();
        if (sim)
            m_world = &sim->GetSimulation().GetWorld();

        // Create widget trees via the UI toolkit
        // auto& ui = ctx.GetAppSubsystem<UIService>();
        // m_hud = ui.CreateLayer(UILayer::HUD);
        // m_panels = ui.CreateLayer(UILayer::Panels);
        // m_modals = ui.CreateLayer(UILayer::Modal);
    }

    void OnDetach(EngineContext& ctx) override
    {
        // Release widget trees
        m_world = nullptr;
    }

    void OnSuspend(EngineContext& ctx) override
    {
        // Pause animations, gray out HUD
    }

    void OnResume(EngineContext& ctx) override
    {
        // Resume animations
    }

    void OnEvent(EngineContext& ctx, Event& event) override
    {
        // Delegate to UI toolkit for hit testing, focus, modals
        // m_ui->DispatchEvent(event);
    }

    void OnUpdate(EngineContext& ctx, float dt) override
    {
        if (not m_world)
            return;

        // Refresh data bindings from ECS
        if (auto* health = m_world->get<PlayerHealth>())
            m_healthBar.SetValue(health->Current, health->Max);
    }

    void OnRender(EngineContext& ctx) override
    {
        // Submit to canvas (or delegate to toolkit which submits for you)
        // m_ui->Render(*m_uiCanvas);
    }

    auto GetName() const -> std::string_view override { return "JourneyGameUI"; }

private:
    UICanvas* m_uiCanvas = nullptr;
    const flecs::world* m_world = nullptr;
    HealthBar m_healthBar;
};
```

Registered by the game plugin:

```cpp
void JourneyGamePlugin::Build(AppBuilder& builder)
{
    builder.ForState<GameplayState>().RegisterStateUI<JourneyGameUI>();
}
```

### ECS-to-UI data flow

```cpp
// ECS system: writes an interaction hint singleton
world.system<Transform, Interactable>()
    .each([](flecs::entity e, Transform& t, Interactable& ia) {
        if (NearPlayer(t))
            e.world().set<InteractionPrompt>({ .Action = ia.Action });
        else
            e.world().remove<InteractionPrompt>();
    });

// IStateUI::OnUpdate reads it and updates widget state:
if (auto* prompt = m_world->get<InteractionPrompt>())
    m_promptWidget.Show(prompt->Action, prompt->ScreenPos);
else
    m_promptWidget.Hide();
```

The engine provides `UICanvas` (rendering submission target), the UI toolkit (widget trees, input, layout), and `IStateUI` (injection point). The game provides the concrete implementation (which panels exist, what data they show, what modals they present). Loading spinners, tutorial popups, quest trackers, inventory screens -- all handled by the `IStateUI` implementation, not overlays.
