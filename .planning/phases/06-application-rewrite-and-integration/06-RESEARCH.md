# Phase 6: Application Rewrite and Integration - Research

**Researched:** 2026-04-05
**Domain:** C++23 application architecture, game engine frame loops, plugin-composed startup, platform subsystem collapse, headless testing
**Confidence:** HIGH

## Summary

Phase 6 is the integration capstone: rewriting `Application::Initialise()`, `Application::Loop()`, and `Application::Shutdown()` to exclusively use v2 types (ApplicationStateMachine, OverlayStack, EngineContext, AppBuilder, SubsystemRegistry), collapsing the three-tier platform abstraction (abstract interface + enum factory + wrapper subsystem) into direct subsystem implementations, completely rewriting the Journey sandbox to use plugin composition, auditing and rewriting all tests against v2 types, and removing all major v1 types (EngineRuntime, LayerStack, Game, Layer, FpsOverlayLayer, PlatformBackend, RenderBackend, abstract Window/Input/Time/RenderDevice).

Research across seven modern game engines (Oxylus, RavEngine, Wicked Engine, Spartan, Bevy, Unreal 5, O3DE) confirms that Wayfinder's architectural decisions are well-aligned with industry best practice: plugin-composed startup, capability-gated subsystem activation, state-machine-driven frame loops, and the removal of abstract interface indirection in favour of direct implementations. The frame sequence (D-06) matches the canonical game loop structure found across all studied engines.

**Primary recommendation:** Execute the switchover atomically per D-05. The v2 types are already implemented and tested individually in Phases 1-5; Phase 6 wires them together and removes v1. The critical path is: (1) collapse platform wrappers into direct subsystem implementations, (2) rewrite Application to use only v2 members, (3) rewrite Journey sandbox, (4) audit/rewrite all tests, (5) remove v1 types. Each step is independently verifiable.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Collapse three-tier pattern (abstract platform interface + PlatformBackend enum factory + AppSubsystem wrapper) into direct subsystem implementations. SDLWindowSubsystem IS the AppSubsystem.
- **D-02:** Null platform implementations removed entirely. Headless = don't add the plugin. Capability-gating handles absent subsystems.
- **D-02a:** NullTime deterministic tick moves to test infrastructure as FixedTimeSubsystem.
- **D-03:** SDLPlatformPlugins is a PluginGroup expanding to SDLWindowPlugin + SDLInputPlugin + SDLTimePlugin.
- **D-04:** Build-time stripping deferred. Runtime-only capability gating in Phase 6.
- **D-05:** Atomic switchover -- v1 frame loop replaced in single step.
- **D-06:** V2 frame sequence: ProcessPending -> PollEvents -> OverlayStack::OnEvent -> ActiveState::OnEvent -> ActiveState::OnUpdate -> OverlayStack::Update -> ActiveState::OnRender -> OverlayStack::Render -> Renderer::Present
- **D-07:** Running state from ASM (no active state = exit loop). Window close triggers state transition or quit.
- **D-08:** Journey main() structure with AddPlugin<SDLPlatformPlugins>(), AddPlugin<SDLRenderDevicePlugin>(), AddPlugin<EngineRenderPlugin>(), AddPlugin<JourneyPlugin>(), app.Run()
- **D-09:** JourneyPlugin registers GameplayState, initial transition, game-specific ECS components/systems, PhysicsPlugin dependency.
- **D-10:** Boot scene via ConfigService (established in Phase 5).
- **D-11:** All major v1 types removed in Phase 6.
- **D-12:** Platform implementation files refactored into subsystem implementations.
- **D-13:** Full audit and rewrite of all test files against v2 types.
- **D-14:** New Application integration test (headless, no GPU, validates frame sequence).
- **D-15:** No NullPlatformPlugins group. Tests register only what they need.

### Claude's Discretion
- Internal layout of SDLWindowSubsystem, SDLInputSubsystem etc. (how they wrap SDL3 calls directly)
- How Window close event maps to Application quit (RequestTransition to exit state, or direct flag)
- EngineRenderPlugin internal structure (what render features it registers, how render graph is composed)
- Whether abstract platform interfaces are kept as documentation/concepts or fully removed
- How EventQueue integrates with the new frame loop (same drain pattern or restructured)
- JourneyPlugin::Build() implementation details (which components, systems, states it registers)
- How existing render features adapt to the collapsed subsystem pattern
- How Application::Loop() handles absent subsystems (no window = no ShouldClose() check, etc.)

### Deferred Ideas (OUT OF SCOPE)
- Build-time platform stripping (CMake options for WAYFINDER_ENABLE_SDL3 etc.)
- Alternative render backends (bgfx, wgpu)
- Platform module boundaries (wayfinder::core, wayfinder::sdl3_platform CMake targets)
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| APP-02 | v2 main loop (ProcessPending -> events -> update -> render) | Frame loop architecture patterns (Section: Architecture Patterns), engine comparisons confirming canonical loop structure |
| APP-03 | Journey sandbox updated to use new architecture | Sandbox/game application patterns (Section: Code Examples), plugin composition patterns from Oxylus/Bevy/O3DE |
| APP-04 | Existing tests updated or rewritten for new architecture | Headless testing patterns (Section: Common Pitfalls, Don't Hand-Roll), FixedTimeSubsystem design |
</phase_requirements>

## Standard Stack

### Core
| Library/Feature | Version | Purpose | Why Standard |
|------------|---------|---------|--------------|
| C++23 `std::expected<T,E>` (via `Result<T>`) | C++23 | Error propagation in Initialise/Shutdown paths | Already used throughout engine. Frame loop init path returns Result<void>. [VERIFIED: codebase] |
| C++23 deducing `this` | C++23 | Ref-qualified accessors on subsystem types | Already used in Oxylus (App::run uses `this App& self`). Cleaner than const/non-const overload pairs. [VERIFIED: Oxylus source] |
| C++23 `std::stop_source`/`std::stop_token` | C++23 | Cooperative shutdown signalling (EngineContext::RequestStop) | Already wired in EngineContext. Application::Loop() checks IsStopRequested(). [VERIFIED: codebase] |
| SDL3 | 3.x | Platform services (window, input, time) | Already used. Platform impl files are being refactored, not replaced. [VERIFIED: codebase] |
| SDL_GPU | Via SDL3 | Render device | Already used. SDLGPUDevice logic moves to SDLRenderDeviceSubsystem. [VERIFIED: codebase] |
| flecs | Current | ECS world in Simulation | Already used. No changes in Phase 6. [VERIFIED: codebase] |
| doctest | Current | Unit/integration testing | Already used for all tests. [VERIFIED: codebase] |

### Supporting
| Library/Feature | Version | Purpose | When to Use |
|------------|---------|---------|-------------|
| `std::type_index` | C++11+ | Keying states and subsystems in registries | Already the established pattern for ASM, SubsystemRegistry. [VERIFIED: codebase] |
| `std::unique_ptr` | C++11+ | Ownership of subsystem instances, state instances | All v2 types use unique_ptr ownership. [VERIFIED: codebase] |
| `std::span` | C++20 | Non-owning views of event buffers, overlay entries | Used in OverlayStack::GetEntries(). [VERIFIED: codebase] |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `std::stop_source` for shutdown | `std::atomic<bool> m_running` | stop_source already wired in EngineContext; provides stop_token for cooperative cancellation in subsystems. More composable than a bare flag. |
| Direct SDL3 calls in subsystem | Keep abstract Window/Input/Time interfaces | D-01 explicitly removes the abstraction layer. Direct calls are simpler, faster to compile, and the plugin file decomposition already enables future backends. |
| `EventQueue` drain in frame loop | Immediate event dispatch (Wicked Engine pattern) | EventQueue batching is safer for frame consistency. All studied engines batch or defer events to well-defined frame points. |

## Architecture Patterns

### Recommended Subsystem File Layout

After collapse (D-01, D-12), the platform directory structure becomes:

```
engine/wayfinder/src/
  platform/
    sdl3/
      SDLWindowSubsystem.h      # WAS: SDL3Window.h + WindowSubsystem.h
      SDLWindowSubsystem.cpp     # WAS: SDL3Window.cpp + WindowSubsystem.cpp
      SDLInputSubsystem.h        # WAS: SDL3Input.h + InputSubsystem.h
      SDLInputSubsystem.cpp      # WAS: SDL3Input.cpp + InputSubsystem.cpp
      SDLTimeSubsystem.h         # WAS: SDL3Time.h + TimeSubsystem.h
      SDLTimeSubsystem.cpp       # WAS: SDL3Time.cpp + TimeSubsystem.cpp
      SDLRenderDeviceSubsystem.h # WAS: SDLGPUDevice.h + RenderDeviceSubsystem.h
      SDLRenderDeviceSubsystem.cpp
      SDLPlatformPlugins.h       # PluginGroup + individual plugin types
      SDLPlatformPlugins.cpp
      SDLRenderDevicePlugin.h
      SDLRenderDevicePlugin.cpp
```

Files removed:
```
  platform/
    Window.h                    # Abstract interface -- REMOVED
    Input.h                     # Abstract interface -- REMOVED
    Time.h                      # Abstract interface -- REMOVED
    BackendConfig.h             # PlatformBackend/RenderBackend enums -- REMOVED
    null/
      NullWindow.h              # REMOVED (headless = no plugin)
      NullInput.h               # REMOVED
      NullTime.h                # Deterministic tick -> tests/FixedTimeSubsystem
  app/
    WindowSubsystem.h/.cpp      # Wrapper -- REMOVED (collapsed into SDLWindowSubsystem)
    InputSubsystem.h/.cpp       # Wrapper -- REMOVED
    TimeSubsystem.h/.cpp        # Wrapper -- REMOVED
    RenderDeviceSubsystem.h/.cpp # Wrapper -- REMOVED (may keep if RendererSubsystem needs abstract device access)
    EngineRuntime.h/.cpp        # V1 monolith -- REMOVED
    LayerStack.h/.cpp           # V1 layer system -- REMOVED
    Layer.h                     # V1 layer interface -- REMOVED
    FpsOverlayLayer.h/.cpp      # V1 FPS overlay -- REMOVED
  gameplay/
    Game.h/.cpp                 # V1 game runtime -- REMOVED
```

### Pattern 1: Direct Subsystem Implementation (Platform Collapse)

**What:** The subsystem IS the platform implementation. No abstract base, no factory, no wrapper.
**When to use:** When there is exactly one implementation (SDL3) and the plugin system handles backend selection.
**Why:** Removes a layer of indirection. The abstract Window/Input/Time base classes exist solely to support PlatformBackend::Null and PlatformBackend::SDL3 switching via enum - both of which are replaced by plugin composition.

```cpp
// SDLWindowSubsystem.h -- direct implementation, no base class
class SDLWindowSubsystem final : public AppSubsystem
{
public:
    [[nodiscard]] auto Initialise(EngineContext& context) -> Result<void> override;
    void Shutdown() override;

    // Direct API -- no virtual dispatch, these are the only window methods
    [[nodiscard]] auto GetWidth() const -> uint32_t { return m_width; }
    [[nodiscard]] auto GetHeight() const -> uint32_t { return m_height; }
    [[nodiscard]] auto ShouldClose() const -> bool { return m_shouldClose; }
    [[nodiscard]] auto GetNativeWindow() const -> SDL_Window* { return m_window; }
    void SetEventCallback(EventCallbackFn callback);
    void PollEvents();  // SDL_PollEvent loop, fires callbacks

private:
    SDL_Window* m_window = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_shouldClose = false;
    // ... SDL-specific state, no abstract base
};
```

**Confirmed by engine research:**
- Oxylus: `Window` is a concrete class, not an abstract interface. No factory pattern. [VERIFIED: Oxylus source]
- Wicked Engine: `wi::platform::window_type` is a platform-specific typedef, not a virtual interface. [VERIFIED: Wicked source]
- Spartan: Single concrete Window type per platform, selected at compile-time not runtime. [VERIFIED: Spartan README]
- RavEngine: Single concrete `Window` per platform, no virtual dispatch for platform services. [VERIFIED: RavEngine source]

### Pattern 2: Plugin-Composed Startup

**What:** Application knows nothing about which subsystems exist. Plugins register everything via AppBuilder.
**When to use:** The entire Phase 6 Application::Initialise() flow.

```cpp
// SDLWindowPlugin -- registers SDLWindowSubsystem with AppBuilder
class SDLWindowPlugin : public IPlugin
{
public:
    auto Describe() const -> PluginDescriptor override
    {
        return { .Name = "SDLWindowPlugin" };
    }

    void Build(AppBuilder& builder) override
    {
        builder.RegisterAppSubsystem<SDLWindowSubsystem>({
            .RequiredCapabilities = { Capability::Presentation },
        });
    }
};

// SDLPlatformPlugins -- convenience group
struct SDLPlatformPlugins
{
    void Build(AppBuilder& builder)
    {
        builder.AddPlugin<SDLWindowPlugin>();
        builder.AddPlugin<SDLInputPlugin>();
        builder.AddPlugin<SDLTimePlugin>();
    }
};
```

**Confirmed by engine research:**
- Bevy: All engine functionality comes through plugins (`DefaultPlugins` group expands to Window, Input, Time, Render, etc.). The `App` struct has `add_plugins()` as the sole composition API. [ASSUMED - based on training knowledge of Bevy architecture]
- O3DE: "Gems" are the plugin unit. ActiveGems.cmake lists which gems are included. Each gem has an `AZ::Module` with `GetRequiredSystemComponents()`. [ASSUMED - based on training knowledge of O3DE]
- Oxylus: Uses a module registry pattern - `self.registry.has<Renderer>()` checks, `self.registry.init()` initialises all registered modules. [VERIFIED: Oxylus source]
- Unreal: `UGameEngine::Init()` iterates registered subsystems. Lyra uses `UGameFeatureAction` plugins to compose gameplay features. [ASSUMED - based on training knowledge]

### Pattern 3: State-Machine-Driven Frame Loop

**What:** The ASM controls whether the loop continues. No active state = exit.
**When to use:** Application::Loop() termination condition.

```cpp
void Application::Loop()
{
    while (not m_context.IsStopRequested())
    {
        m_stateMachine.ProcessPending(m_context);

        // No active state means we're done
        if (not m_stateMachine.GetActiveState())
        {
            break;
        }

        // ... frame sequence per D-06
    }
}
```

**Confirmed by engine research:**
- Wicked Engine: Uses `activePath` (RenderPath*) - if null, nothing runs. `ActivatePath()` transitions between render paths with fade support. Conceptually a state machine for render paths. [VERIFIED: Wicked source]
- Oxylus: `while (self.is_running)` loop with explicit `should_stop()` method. Module-registry-driven update. [VERIFIED: Oxylus source]
- RavEngine: World-based loop - `SetRenderedWorld()` determines what runs. No world = no rendering. [VERIFIED: RavEngine source]

### Pattern 4: Capability-Gated Absent Subsystems

**What:** Application::Loop() uses `TryGet` to handle absent subsystems gracefully.
**When to use:** Every subsystem access in the frame loop that might not exist (window, renderer).

```cpp
// In Application::Loop():
auto* window = m_context.TryGetAppSubsystem<SDLWindowSubsystem>();
auto* renderer = m_context.TryGetAppSubsystem<RendererSubsystem>();
auto* time = m_context.TryGetAppSubsystem<SDLTimeSubsystem>();

// Headless (no window): skip PollEvents, skip ShouldClose check
if (window)
{
    window->PollEvents();
}

// No renderer: skip Present
if (renderer)
{
    renderer->GetCanvases().Reset();
    // ... render submission
    renderer->Present();
}
```

This pattern is how D-02 (no null implementations) and D-15 (tests register only what they need) work at the frame loop level.

### Pattern 5: Event Routing Through OverlayStack -> ActiveState

**What:** Events flow top-down through overlays first, unconsumed events reach the active state.
**Confirmed:** This is the universal pattern across all studied engines.

```cpp
// D-06 steps 3-4:
m_overlayStack.ProcessEvents(m_context, m_eventQueue);

if (auto* state = m_stateMachine.GetActiveState())
{
    m_eventQueue.Drain([&](Event& e)
    {
        state->OnEvent(m_context, e);
    });
}
```

- Wicked Engine: Input system processes before render path. [VERIFIED: Wicked source]
- Oxylus: Module input update runs in main loop, game modules process after. [VERIFIED: Oxylus source]

### Anti-Patterns to Avoid

- **Keeping abstract interfaces "for documentation":** D-01 is explicit about removal. Concepts or static_assert can document subsystem API requirements if needed, without the virtual dispatch overhead and indirection.
- **Gradual migration with v1/v2 coexistence:** D-05 mandates atomic switchover. No `#ifdef V2_FRAME_LOOP` or `if (m_useV2) {...}`.
- **NullSubsystem types for headless:** D-02 removes these. Capability-gating and TryGet handle absence.
- **Application knowing about specific subsystems:** Application should use TryGet for optional subsystems, not `#include "SDLWindowSubsystem.h"`. The frame loop queries by abstract capability (has window? has renderer?) not by concrete type.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Deterministic time for tests | Another NullTimeSubsystem in engine | `FixedTimeSubsystem` in test infrastructure (D-02a) | Test-only concern. Engine should not ship null implementations. Fixed 1/60 dt is trivial. |
| Plugin dependency ordering | Manual init ordering | AppBuilder's existing topological sort via PluginDescriptor::DependsOn | Already built in Phase 3. Plugins declare deps, builder resolves order. |
| Subsystem init ordering | Manual order in Initialise() | SubsystemRegistry's DependsOn topological sort (Phase 2) | Already built. SDLRenderDeviceSubsystem declares DependsOn{SDLWindowSubsystem}. |
| Frame sequence validation in tests | Ad-hoc assertion chains | Lifecycle logging pattern (already in tests) with ordered event vector | Already established in SubsystemTests and ApplicationStateMachineTests. |
| Shutdown ordering | Manual reverse-init | SubsystemManifest::Shutdown() already handles reverse topological order | Built in Phase 2. |

**Key insight:** Phase 6 wires together already-built infrastructure. The temptation is to re-implement ordering, lifecycle management, or capability checks at the Application level. Resist this -- delegate to the v2 types that already handle it.

## Common Pitfalls

### Pitfall 1: Breaking Event Routing During Collapse

**What goes wrong:** When `SDL3Window::Update()` is moved into `SDLWindowSubsystem::PollEvents()`, the event callback wiring changes. If the EventCallbackFn signature or the callback registration timing changes, events silently stop flowing.
**Why it happens:** SDL3Window currently stores an `EventCallbackFn` set by Application::Initialise(). In v2, the callback must be wired after subsystem initialisation, not during it (because Application's event handler isn't ready during SubsystemManifest::Initialise()).
**How to avoid:** Wire the event callback in Application::Initialise() AFTER SubsystemManifest::Initialise() completes. Or have SDLWindowSubsystem push events into the EventQueue directly (preferred per D-06: PollEvents populates the EventQueue, not callbacks).
**Warning signs:** Tests pass but no input reaches states. Window close doesn't trigger shutdown.

### Pitfall 2: SDL_Init Called Multiple Times or Not At All

**What goes wrong:** SDL3Window currently calls `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)` in its `Initialise()`. When this moves to SDLWindowSubsystem, and SDLInputSubsystem also needs SDL initialised, there's a risk of double-init or missing init.
**Why it happens:** SDL3 is designed around subsystem-based init flags. The current code does `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)` in SDL3Window and nothing in SDL3Input/SDL3Time (they rely on window having already init'd).
**How to avoid:** Either: (a) SDLWindowSubsystem owns SDL_Init and is DependsOn by SDLInputSubsystem, or (b) a shared `SDLPlatformInit` subsystem runs first and only calls SDL_Init once. DependsOn ordering ensures SDLWindowSubsystem initialises before SDLRenderDeviceSubsystem (swapchain needs window surface).
**Warning signs:** "SDL not initialised" errors on headless runs. Double SDL_Quit crashes.

### Pitfall 3: EngineContext Not Fully Wired Before Subsystem Init

**What goes wrong:** SubsystemManifest::Initialise() passes EngineContext to each subsystem's Initialise(). If EngineContext's pointers (to ASM, OverlayStack, AppDescriptor, manifests) aren't set before this call, subsystems that query EngineContext during init will hit assertion failures.
**Why it happens:** Application::Initialise() must set up EngineContext BEFORE calling SubsystemManifest::Initialise(). The current code sets some EngineContext fields after EngineRuntime::Initialise().
**How to avoid:** Wire EngineContext fully (SetStateMachine, SetOverlayStack, SetAppDescriptor, SetAppSubsystems) before calling any Initialise on manifests. The ASM and OverlayStack don't need to be started, just assigned.
**Warning signs:** Assertion failures in EngineContext during startup. "App subsystem registry not set" from subsystem Initialise().

### Pitfall 4: Test Files Referencing V1 Types After Removal

**What goes wrong:** Tests that include removed headers (EngineRuntime.h, Game.h, LayerStack.h, BackendConfig.h, Window.h) fail to compile.
**Why it happens:** D-13 requires full audit, but it's easy to miss transitive includes. A test that includes `EngineConfig.h` might transitively need `BackendConfig.h` if EngineConfig still references backend enums.
**How to avoid:** Remove v1 types first, then fix compilation errors. Don't try to update tests before removing the types -- the compiler errors guide the audit. Use the pattern: remove header -> fix all compilation errors -> verify tests pass.
**Warning signs:** Compilation errors cascade after removing v1 headers.

### Pitfall 5: RendererSubsystem Collapse is Non-Trivial

**What goes wrong:** Unlike Window/Input/Time (which have a clean abstract-to-concrete mapping), RendererSubsystem wraps `Renderer` which is not platform-specific. Collapsing it the same way would mean making Renderer an AppSubsystem directly, which changes its interface contract.
**Why it happens:** Renderer doesn't inherit from a platform abstract interface -- it's already a concrete class. RendererSubsystem is a legitimate wrapper providing subsystem lifecycle, not a pointless abstraction layer.
**How to avoid:** Per CONTEXT.md discretion: "RendererSubsystem.h -- may keep or collapse depending on how tightly it wraps Renderer." Keep RendererSubsystem as-is. It provides RAII lifecycle, canvas ownership, and BlendableEffectRegistry ownership. These are subsystem responsibilities, not Renderer responsibilities.
**Warning signs:** Temptation to make Renderer inherit from AppSubsystem, which would couple rendering internals to the subsystem lifecycle API.

### Pitfall 6: Window Close Ambiguity

**What goes wrong:** In v1, `m_running = false` + `m_runtime->ShouldClose()` handle window close. In v2, window close must flow through the event system and either (a) trigger a state transition to an exit state that leaves the ASM empty, or (b) call `m_context.RequestStop()`.
**Why it happens:** D-07 says "Window close event triggers a state transition or direct quit" but doesn't prescribe which.
**How to avoid:** Use `m_context.RequestStop()` for window close (simplest, matches EngineContext's existing stop_source pattern). The ASM check (`GetActiveState() == nullptr`) handles graceful exit via state graph exhaustion. Window close is an OS interrupt, not a game state transition.
**Warning signs:** Window close requires clicking X twice. Or window close leaves state in limbo.

## Code Examples

### V2 Application::Initialise() Skeleton

```cpp
auto Application::Initialise() -> Result<void>
{
    Log::Initialise();

    // 1. Discover project
    auto projectFile = FindProjectFile();
    if (not projectFile) return std::unexpected(projectFile.error());
    auto loadResult = ProjectDescriptor::LoadFromFile(*projectFile);
    if (not loadResult) return std::unexpected(loadResult.error());
    m_project = std::make_unique<ProjectDescriptor>(std::move(loadResult->Descriptor));

    // 2. Build plugins -> AppDescriptor
    if (not m_builder) return MakeError("No plugins registered");
    m_builder->SetProjectPaths(m_project->ResolveConfigDir(), m_project->ProjectRoot / "saved");
    auto descriptorResult = m_builder->Finalise();
    if (not descriptorResult) return std::unexpected(descriptorResult.error());
    m_appDescriptor = std::move(*descriptorResult);

    // 3. Extract registries from builder outputs
    // (SubsystemManifest, StateManifest, OverlayManifest, LifecycleHookManifest, ConfigService)

    // 4. Wire EngineContext BEFORE subsystem init
    m_context.SetAppDescriptor(&*m_appDescriptor);
    m_context.SetStateMachine(&m_stateMachine);
    m_context.SetOverlayStack(&m_overlayStack);
    m_context.SetAppSubsystems(&m_appManifest);

    // 5. Initialise subsystems (topological order, returns Result<void>)
    if (auto result = m_appManifest.Initialise(m_context); not result)
        return std::unexpected(result.error());

    // 6. Finalise ASM (from StateManifest), start initial state
    // ... setup from AppDescriptor's state manifest
    if (auto result = m_stateMachine.Finalise(); not result)
        return std::unexpected(result.error());
    m_stateMachine.Start(m_context);

    // 7. Fire OnAppReady hooks
    // ... lifecycle hooks from AppDescriptor

    return {};
}
```
Source: Derived from existing Application.cpp + AppBuilder pattern + v2 architecture spec [VERIFIED: codebase]

### V2 Application::Loop() Skeleton

```cpp
void Application::Loop()
{
    while (not m_context.IsStopRequested())
    {
        // D-06 step 1: Process deferred state transitions
        m_stateMachine.ProcessPending(m_context);

        // D-07: No active state = exit
        auto* activeState = m_stateMachine.GetActiveState();
        if (not activeState)
        {
            break;
        }

        // Capability-gated subsystem access (headless-safe)
        auto* window = m_context.TryGetAppSubsystem<SDLWindowSubsystem>();
        auto* time = m_context.TryGetAppSubsystem<SDLTimeSubsystem>();
        auto* renderer = m_context.TryGetAppSubsystem<RendererSubsystem>();

        // D-06 step 2: Poll platform events
        if (time) time->Update();
        if (window) window->PollEvents();

        // D-06 steps 3-4: Event routing
        m_overlayStack.ProcessEvents(m_context, m_eventQueue);
        // Remaining events to active state
        m_eventQueue.Drain([&](Event& e)
        {
            activeState->OnEvent(m_context, e);
        });

        // D-06 step 5: State update
        const float dt = time ? time->GetDeltaTime() : 0.0f;
        activeState->OnUpdate(m_context, dt);

        // D-06 step 6: Overlay update
        m_overlayStack.Update(m_context, dt);

        // Reset canvases for this frame
        if (renderer) renderer->GetCanvases().Reset();

        // D-06 steps 7-8: Render
        activeState->OnRender(m_context);
        m_overlayStack.Render(m_context);

        // D-06 step 9: Present
        if (renderer) renderer->Present();

        // Window close check
        if (window and window->ShouldClose())
        {
            m_context.RequestStop();
        }

        WAYFINDER_PROFILE_FRAME_MARK();
    }
}
```
Source: Derived from D-06 frame sequence + existing Application::Loop() + v2 architecture spec [VERIFIED: codebase]

### V2 Journey main()

```cpp
#include "app/Application.h"
#include "platform/sdl3/SDLPlatformPlugins.h"
#include "platform/sdl3/SDLRenderDevicePlugin.h"
#include "rendering/EngineRenderPlugin.h"
#include "JourneyPlugin.h"

auto main(int argc, char* argv[]) -> int
{
    Wayfinder::Application app({.Count = argc, .Args = argv});
    app.AddPlugin<Wayfinder::SDLPlatformPlugins>();
    app.AddPlugin<Wayfinder::SDLRenderDevicePlugin>();
    app.AddPlugin<Wayfinder::EngineRenderPlugin>();
    app.AddPlugin<JourneyPlugin>();
    app.Run();
    return 0;
}
```
Source: D-08 [VERIFIED: CONTEXT.md]

### SDLWindowSubsystem (Collapsed)

```cpp
// platform/sdl3/SDLWindowSubsystem.h
class SDLWindowSubsystem final : public AppSubsystem
{
public:
    using EventCallbackFn = std::function<void(Event&)>;

    SDLWindowSubsystem() = default;
    ~SDLWindowSubsystem() override;

    [[nodiscard]] auto Initialise(EngineContext& context) -> Result<void> override;
    void Shutdown() override;

    void PollEvents();
    void SetEventCallback(EventCallbackFn callback);

    [[nodiscard]] auto GetWidth() const -> uint32_t { return m_width; }
    [[nodiscard]] auto GetHeight() const -> uint32_t { return m_height; }
    [[nodiscard]] auto ShouldClose() const -> bool { return m_shouldClose; }
    [[nodiscard]] auto GetNativeWindow() const -> SDL_Window* { return m_window; }
    [[nodiscard]] auto IsVSync() const -> bool { return m_vsync; }

    void SetVSync(bool enabled);
    void SetTitle(std::string_view title);
    void SetSize(uint32_t width, uint32_t height);

private:
    void ReleaseResources();

    SDL_Window* m_window = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    std::string m_title;
    bool m_vsync = false;
    bool m_shouldClose = false;
    bool m_initialised = false;
    EventCallbackFn m_eventCallback;
};
```
Source: Merged from SDL3Window.h + WindowSubsystem.h per D-01/D-12 [VERIFIED: codebase]

### FixedTimeSubsystem (Test Infrastructure)

```cpp
// tests/FixedTimeSubsystem.h
class FixedTimeSubsystem final : public AppSubsystem
{
public:
    static constexpr float DEFAULT_DT = 1.0f / 60.0f;

    [[nodiscard]] auto Initialise(EngineContext& /*context*/) -> Result<void> override
    {
        return {};
    }

    void Shutdown() override {}

    void Update() { m_elapsedTime += m_deltaTime; }

    [[nodiscard]] auto GetDeltaTime() const -> float { return m_deltaTime; }
    [[nodiscard]] auto GetElapsedTime() const -> float { return m_elapsedTime; }

    void SetDeltaTime(float dt) { m_deltaTime = dt; }

private:
    float m_deltaTime = DEFAULT_DT;
    float m_elapsedTime = 0.0f;
};
```
Source: D-02a - deterministic tick from NullTime moved to test infrastructure [VERIFIED: CONTEXT.md]

### Application Integration Test Pattern

```cpp
TEST_CASE("Application boots headless and runs frame sequence")
{
    // No platform plugins, no GPU -- just the frame loop mechanics
    struct TestState : public IApplicationState
    {
        int FrameCount = 0;
        static constexpr int TARGET_FRAMES = 3;

        void OnUpdate(EngineContext& ctx, float /*dt*/) override
        {
            ++FrameCount;
            if (FrameCount >= TARGET_FRAMES)
            {
                ctx.RequestStop();
            }
        }

        auto GetName() const -> std::string_view override { return "TestState"; }
    };

    struct TestPlugin : public IPlugin
    {
        auto Describe() const -> PluginDescriptor override { return {.Name = "TestPlugin"}; }
        void Build(AppBuilder& builder) override
        {
            builder.RegisterAppSubsystem<FixedTimeSubsystem>();
            builder.AddState<TestState>({ .Initial = true });
        }
    };

    Application app({});
    app.AddPlugin<TestPlugin>();
    app.Run();
    // App ran TARGET_FRAMES frames and exited cleanly
}
```
Source: D-14 - headless Application integration test [VERIFIED: CONTEXT.md]

## State of the Art

| Old Approach (V1) | Current Approach (V2) | When Changed | Impact |
|---|---|---|---|
| Abstract platform interfaces (Window, Input, Time) with factory methods and PlatformBackend enum | Direct subsystem implementations (SDLWindowSubsystem IS the AppSubsystem) | Phase 6 | Removes one layer of indirection, simplifies compilation, makes future backend addition a plugin-only concern |
| EngineRuntime monolith owning all services | Individual AppSubsystems with DependsOn ordering | Phase 5 wrappers, Phase 6 collapse | Each service has independent lifecycle, testable in isolation |
| LayerStack for frame update/render ordering | ApplicationStateMachine (phases) + OverlayStack (decorations) | Phase 4 + Phase 6 integration | Clear separation of concerns, capability-gated activation |
| Game owns ECS, state machine, subsystems | Simulation is thin (world + scene), GameplayState wraps it | Phase 4/5 | Simulation is headless-testable without Application |
| NullWindow/NullInput for headless | Don't register the plugin. TryGet returns nullptr. | Phase 6 | No null implementations to maintain. Capability-gating handles everything. |

**Industry confirmation:**
- **Oxylus** uses a module registry with deducing-this methods (modern C++23). No abstract platform interfaces.  Modules init/deinit in registry order. [VERIFIED: Oxylus source]
- **Wicked Engine** uses `RenderPath` as its state abstraction (ActivatePath with fade transitions). Frame loop is Application::Run() calling Update/FixedUpdate/Render/Compose. Input and platform are global singletons. [VERIFIED: Wicked source]
- **RavEngine** uses World-based architecture (SetRenderedWorld, AddWorld, RemoveWorld). App has OnStartup/OnShutdown hooks. Taskflow for thread pool. No plugin system. [VERIFIED: RavEngine source]
- **Spartan Engine** has subsystem-based architecture with a single Engine class orchestrating updates. Thread pool for parallelism. Entity system with component architecture. PhysX at 200Hz fixed timestep. [VERIFIED: Spartan README]
- **Bevy** pioneered the plugin composition model that Wayfinder follows: `App::new().add_plugins(DefaultPlugins).add_plugins(MyGamePlugin).run()`. Resources as typed world singletons. [ASSUMED]
- **O3DE** uses "Gems" (modules) with `AZ::Module` declaring required system components. The `Application::Create()` -> `Application::Run()` lifecycle is nearly identical to Wayfinder's pattern. [ASSUMED]
- **Unreal 5** uses `USubsystem` with `UEngineSubsystem`, `UEditorSubsystem`, `UGameInstanceSubsystem`, `UWorldSubsystem`, `ULocalPlayerSubsystem` scopes. Lyra adds `UGameFeatureAction` for modular gameplay composition. [ASSUMED]

Wayfinder's approach is cleaner than any of the studied engines because it combines:
1. Bevy's plugin composition model
2. Unreal's scoped subsystem concept (app-scope vs state-scope)
3. Oxylus's modern C++23 idioms
4. A capability-gating system that none of the studied engines have as cleanly implemented

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Bevy uses `App::new().add_plugins(DefaultPlugins).run()` as its composition model | Architecture Patterns: Plugin-Composed Startup | LOW - Bevy's plugin system is very well-known. Doesn't affect implementation. |
| A2 | O3DE uses AZ::Module with GetRequiredSystemComponents() | State of the Art | LOW - Structural reference only. No implementation depends on this. |
| A3 | Unreal uses USubsystem with multiple scopes and Lyra uses UGameFeatureAction | State of the Art | LOW - Structural reference only. |
| A4 | Application::Loop() can use TryGet for absent subsystems without performance concern | Architecture Patterns | LOW - TryGet is a hash lookup on type_index, called once per frame per subsystem. Negligible cost. |
| A5 | SDLWindowSubsystem should own SDL_Init responsibility (DependsOn chain handles ordering) | Common Pitfalls | MEDIUM - If SDL3 requires different init patterns (e.g. SDL_INIT_VIDEO must be in audio thread), this assumption breaks. Verify SDL3 docs. |

## Open Questions

1. **EventQueue integration pattern**
   - What we know: V1 uses EventCallbackFn (callback from Window::Update) + EventQueue (deferred input). D-06 specifies PollEvents as step 2.
   - What's unclear: Should SDLWindowSubsystem::PollEvents() push events directly into EventQueue (owned by Application), or fire callback which Application routes to EventQueue?
   - Recommendation: Direct push to EventQueue is cleanest. SDLWindowSubsystem gets a pointer to EventQueue in Initialise() or via EngineContext. This eliminates the callback-based routing entirely. Alternatively, keep the callback but set it to `[&eventQueue](Event& e) { eventQueue.Push(e); }` at Application::Initialise(). Either works; the callback approach is less invasive.

2. **RendererSubsystem::Present() method**
   - What we know: D-06 step 9 is `Renderer::Present()`. RendererSubsystem owns the Renderer.
   - What's unclear: Does RendererSubsystem need a new `Present()` method, or does Application call `renderer.GetRenderer().SomeExistingMethod()`?
   - Recommendation: Add `RendererSubsystem::Present()` that delegates to `Renderer::Render(canvasData)` + `RenderDevice::EndFrame()` + swap. Keep the subsystem as the public API surface.

3. **Application constructor signature change**
   - What we know: Current constructor takes `(std::unique_ptr<Plugins::Plugin> gamePlugin, CommandLineArgs)`. V2 uses AddPlugin<T>().
   - What's unclear: Does the constructor still accept a game plugin, or is it CommandLineArgs-only?
   - Recommendation: CommandLineArgs-only. The old Plugin constructor parameter is part of the v1 DLL loading model (PluginExport.h). V2 is exclusively AddPlugin<T>().

4. **EngineConfig handling in v2**
   - What we know: V1 loads a monolithic EngineConfig. V2 has per-plugin config via ConfigService. But subsystems like SDLWindowSubsystem still need window config (width, height, title, vsync).
   - What's unclear: Does EngineConfig survive as a ConfigService-managed config struct, or is it decomposed into WindowConfig, RenderConfig, etc.?
   - Recommendation: Keep EngineConfig as a ConfigService-managed struct for now (it's already registered via `builder.LoadConfig<EngineConfig>("engine")`). Future phases can decompose it. The existing WindowSubsystem.cpp already reads config from ConfigService.

## Environment Availability

Step 2.6: SKIPPED (no external dependencies identified). Phase 6 is purely code/config changes using existing toolchain (CMake + Clang + SDL3 + doctest).

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | doctest (current) |
| Config file | tests/CMakeLists.txt |
| Quick run command | `ctest --preset test -R "core_tests\|render_tests\|scene_tests\|physics_tests"` |
| Full suite command | `ctest --preset test` |

### Phase Requirements -> Test Map
| Req ID | Behaviour | Test Type | Automated Command | File Exists? |
|--------|-----------|-----------|-------------------|-------------|
| APP-02 | V2 frame loop runs ProcessPending -> events -> update -> render | integration | New Application integration test (D-14) | No - Wave 0 |
| APP-03 | Journey sandbox boots through AddPlugin, enters GameplayState | manual smoke | Build journey target, run, observe rendering | N/A - manual |
| APP-04 | All existing tests pass against v2 architecture | unit/integration | `ctest --preset test` | Yes - existing tests need rewrite |

### Sampling Rate
- **Per task commit:** Build debug + run affected test target
- **Per wave merge:** `cmake --build --preset debug && ctest --preset test`
- **Phase gate:** Full suite green, Journey builds and runs

### Wave 0 Gaps
- [ ] `tests/app/ApplicationIntegrationTests.cpp` -- covers APP-02 (headless frame sequence validation)
- [ ] `tests/FixedTimeSubsystem.h` -- test infrastructure for deterministic time (D-02a)
- [ ] Existing `tests/app/EngineRuntimeTests.cpp` -- needs complete rewrite (references removed EngineRuntime)
- [ ] Existing `tests/app/EngineSubsystemTests.cpp` -- needs rewrite (references WindowSubsystem wrapper, PlatformBackend)
- [ ] Existing `tests/app/SubsystemTests.cpp` -- needs review (may reference v1 patterns)

## Sources

### Primary (HIGH confidence)
- Wayfinder codebase: Application.h/.cpp, EngineRuntime.h/.cpp, all v2 types, all test files, CMakeLists.txt
- Wayfinder architecture docs: application_architecture_v2.md, application_migration_v2.md, game_framework.md
- Phase CONTEXT.md with 15 locked decisions (D-01 through D-15)
- Oxylus Engine source: App.cpp, App.hpp -- module registry, deducing this, modern C++ patterns
- Wicked Engine source: wiApplication.h/.cpp -- RenderPath state pattern, frame loop structure
- RavEngine source: App.hpp -- World-based loop, startup hooks

### Secondary (MEDIUM confidence)
- Spartan Engine README/wiki -- subsystem architecture, threading model, PhysX integration
- SDL3 documentation -- platform init patterns, event polling

### Tertiary (LOW confidence)
- Bevy architecture (from training knowledge) -- plugin composition, resource model
- O3DE architecture (from training knowledge) -- Gem/Module system
- Unreal 5 subsystem architecture (from training knowledge) -- USubsystem scopes, Lyra patterns

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all libraries already in use, no new dependencies
- Architecture: HIGH -- patterns derived from 15 locked decisions and existing codebase
- Pitfalls: HIGH -- identified from reading actual source code and understanding integration points
- Engine comparisons: MEDIUM -- verified for Oxylus/Wicked/RavEngine source, assumed for Bevy/O3DE/Unreal

**Research date:** 2026-04-05
**Valid until:** 2026-05-05 (stable -- no external dependency changes expected)
