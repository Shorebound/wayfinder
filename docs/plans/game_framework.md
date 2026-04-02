# Game Framework

**Status:** Planned
**Parent document:** [application_architecture_v2.md](application_architecture_v2.md)
**Last updated:** 2026-04-02

How `Game` works as a standalone simulation engine, how `GameplayState` and `EditorState` adapt it into the application lifecycle, and the service provider pattern that keeps `Game` framework-agnostic.

---

## Game: Standalone Simulation Engine

`Game` owns the flecs world, state-scoped subsystems, scenes, and the gameplay state machine. It is usable without `Application`, states, overlays, or a window -- headless tests, CLI tools, and the editor all use `Game` directly.

`Game` doesn't know where its services come from. It declares needs via a concept:

```cpp
template<typename T>
concept ServiceProvider = requires(T provider) {
    { provider.template Get<AssetService>() } -> std::same_as<AssetService&>;
    { provider.template TryGet<AssetService>() } -> std::same_as<AssetService*>;
};
```

```cpp
class Game
{
public:
    explicit Game(const AppDescriptor& descriptor);

    auto Initialise(ServiceProvider auto& services) -> Result<void>;
    void Update(float deltaTime);
    void Shutdown();

    auto GetWorld() -> flecs::world&;
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
    const AppDescriptor& m_descriptor;
};
```

`Game::Initialise` pulls what it needs from whatever service provider is given:

```cpp
auto Game::Initialise(ServiceProvider auto& services) -> Result<void>
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

```cpp
struct StandaloneServiceProvider
{
    AssetService* Assets = nullptr;
    GameplayTagRegistry* Tags = nullptr;

    template<typename T>
    auto TryGet() -> T*
    {
        if constexpr (std::same_as<T, AssetService>) return Assets;
        else if constexpr (std::same_as<T, GameplayTagRegistry>) return Tags;
        else return nullptr;
    }

    template<typename T>
    auto Get() -> T&
    {
        if (auto* ptr = TryGet<T>())
            return *ptr;
        throw std::runtime_error("Service not available");
    }
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

StandaloneServiceProvider services{
    .Assets = &assetService,
    .Tags = &tagRegistry,
};
Game game(descriptor);
game.Initialise(services);
game.Update(1.0f / 60.0f);
```

---

## GameplayState: The Application Adapter

Wraps `Game` into the `IApplicationState` lifecycle:

```cpp
class GameplayState : public IApplicationState
{
public:
    auto OnEnter(EngineContext& ctx) -> Result<void> override
    {
        m_game = std::make_unique<Game>(ctx.GetAppDescriptor());
        EngineContextServiceProvider services{ .Ctx = ctx };
        auto result = m_game->Initialise(services);
        if (not result)
            return std::unexpected(result.error());

        StateSubsystems::Bind(&m_game->GetSubsystems());
        return {};
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
        if (m_game->IsGameOver())
            ctx.RequestTransition<CreditsState>();
    }

    void OnRender(EngineContext& ctx) override
    {
        auto& renderer = ctx.GetAppSubsystem<Renderer>();
        if (const auto* scene = m_game->GetCurrentScene())
            renderer.SubmitScene(*scene);
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
    std::unique_ptr<Game> m_game;
};
```

---

## Game-Level State Machine

Game-level states (Playing, Loading, Cutscene, Dialogue) are handled by `GameStateMachine` inside `Game` -- separate from application states. They control ECS system enable/disable via run conditions.

```cpp
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

Application and game state machines share a generic base:

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
};
```

`GameStateMachine` extends with ECS-specific functionality: run conditions, system binding, world reference. `ApplicationStateMachine` extends with typed C++ state objects, push/pop, and the rich lifecycle.

---

## Editor Architecture

### EditorState

The editor is an application state, not an overlay. It **inverts** the hierarchy: `EditorState` contains the game as a viewport rather than overlaying on top of it.

```
GameplayState:   Game renders to swapchain directly
EditorState:     Game renders to offscreen target; editor composites it into a viewport
```

`EditorState` wraps the same `Game` class as `GameplayState`:

```cpp
class EditorState : public IApplicationState
{
public:
    auto OnEnter(EngineContext& ctx) -> Result<void> override
    {
        m_game = std::make_unique<Game>(ctx.GetAppDescriptor());
        EngineContextServiceProvider services{ .Ctx = ctx };
        auto result = m_game->Initialise(services);
        if (not result)
            return std::unexpected(result.error());

        m_panels = std::make_unique<EditorPanelManager>();
        return {};
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
        if (const auto* scene = m_game->GetCurrentScene())
            renderer.SubmitScene(*scene, m_offscreenTarget);
        m_panels->Render(ctx);
    }

private:
    std::unique_ptr<Game> m_game;
    std::unique_ptr<EditorPanelManager> m_panels;
    PlayMode m_playMode = PlayMode::Paused;
};
```

### EditorPanelManager

A thin layout orchestrator within `EditorState`. Handles docking, panel registration, and layout persistence. Editor panels are **not** overlays -- they need docking, persistence, and serialisation. Game overlays (FPS counter, debug console) render on top of everything, including the editor.

The UI toolkit is a shared engine service used by game HUD, menus, and editor panels alike. `EditorPanelManager` is just the docking/arrangement layer on top of it.

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

Same `Application` type, same engine, different plugin sets. `Game` is framework-agnostic -- both `GameplayState` and `EditorState` wrap it. A future separate editor executable can also wrap `Game` directly without changing this design.
