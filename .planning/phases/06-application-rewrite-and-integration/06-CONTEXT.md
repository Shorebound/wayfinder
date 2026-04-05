# Phase 6: Application Rewrite and Integration - Context

**Gathered:** 2026-04-05
**Status:** Ready for planning

<domain>
## Phase Boundary

Application runs entirely on v2 architecture. The v1 frame loop, EngineRuntime, LayerStack, Game, old Plugin base, and FpsOverlayLayer are removed and replaced. The Phase 5 AppSubsystem-wraps-factory pattern is collapsed into direct platform subsystem implementations (SDLWindowSubsystem etc.) removing the PlatformBackend/RenderBackend enum indirection. Journey sandbox boots through `AddPlugin<T>()`, enters GameplayState, and renders frames. All existing tests are audited and rewritten against v2 types. A new Application integration test validates the full boot-to-frame cycle. Phase 7 becomes an audit pass for dead includes and vestigial references only.

**Requirements:** APP-02, APP-03, APP-04

</domain>

<decisions>
## Implementation Decisions

### Platform Subsystem Collapse
- **D-01:** Collapse the three-tier pattern (abstract platform interface + PlatformBackend enum factory + AppSubsystem wrapper) into direct subsystem implementations. E.g., `SDLWindowSubsystem` IS the AppSubsystem -- it directly creates and manages the SDL3 window without going through `Window::Create(PlatformBackend::SDL3)`. The abstract `Window`/`Input`/`Time` base classes and `PlatformBackend`/`RenderBackend` enum dispatch are removed.
- **D-02:** Null platform implementations (NullWindow, NullInput, NullDevice) are removed entirely. In v2, headless = don't add the plugin. If no SDLPlatformPlugins is registered, those subsystems simply don't exist in the registry. Capability-gating means nothing requiring Presentation/Rendering activates. No NullWindowSubsystem, NullInputSubsystem, or NullRenderDeviceSubsystem needed.
- **D-02a:** NullTime's deterministic tick (fixed 1/60f per frame) is valuable for reproducible test behaviour. It moves to test infrastructure as `FixedTimeSubsystem` -- a lightweight AppSubsystem living in the test helpers, not shipped with the engine. Integration tests that need frame-by-frame time control register it via a test plugin.
- **D-03:** SDLPlatformPlugins is a plugin group (Phase 3 PluginGroup) that expands to SDLWindowPlugin + SDLInputPlugin + SDLTimePlugin. Three individual plugins, one convenience group. SDLRenderDevicePlugin and RendererPlugin are separate (render backend is GPU-specific, not OS-specific).
- **D-04:** Build-time stripping (CMake options to exclude SDL code from binary) is deferred to a future platform phase. Phase 6 uses runtime-only capability gating -- unused subsystems are compiled but never activated. The plugin file decomposition makes future build-time stripping trivial.

### Frame Loop Rewrite
- **D-05:** Atomic switchover. The v1 frame loop (EngineRuntime::BeginFrame -> EventQueue drain -> LayerStack update -> Game update -> render) is replaced in a single step with the v2 sequence. No incremental migration, no feature flags, no v1/v2 coexistence.
- **D-06:** V2 frame sequence order:
  1. `ProcessPending()` -- execute deferred state transitions (ASM)
  2. `PollEvents()` -- gather SDL events into EventQueue
  3. `OverlayStack::OnEvent(events)` -- overlays consume events top-down (highest first)
  4. `ActiveState::OnEvent(remaining)` -- active state gets unconsumed events
  5. `ActiveState::OnUpdate(dt)` -- simulation/gameplay tick
  6. `OverlayStack::Update(dt)` -- overlays read fresh data (e.g., PerformanceOverlay reads frame timing)
  7. `ActiveState::OnRender()` -- state fills canvases (SceneCanvas, etc.)
  8. `OverlayStack::Render()` -- overlays draw on top (UICanvas for ImGui)
  9. `Renderer::Present()` -- submit canvases to GPU, swap buffers
- **D-07:** Running state controlled by ApplicationStateMachine. When the ASM has no active state (final state exited, or explicit quit), Application::Loop() exits. No `Game::IsRunning()` equivalent. Window close event triggers a state transition or direct quit.

### Journey Sandbox Migration
- **D-08:** Journey main() structure:
  ```
  app.AddPlugin<SDLPlatformPlugins>();    // Window + Input + Time (plugin group)
  app.AddPlugin<SDLRenderDevicePlugin>(); // GPU device (SDL_GPU)
  app.AddPlugin<EngineRenderPlugin>();    // Renderer + render graph + render features
  app.AddPlugin<JourneyPlugin>();         // GameplayState, components, systems, initial state
  app.Run();
  ```
- **D-09:** JourneyPlugin registers GameplayState as a state, declares the initial transition, registers game-specific ECS components and systems, and registers PhysicsPlugin as a dependency. The core game setup (what JourneyGame::OnStartup currently does) moves into the plugin's Build() method using the registrar API.
- **D-10:** Boot scene loading flows through ConfigService (established in Phase 5 D-02). Simulation reads boot scene path from `config/simulation.toml`. No change from Phase 5 design.

### V1 Code Removal
- **D-11:** All major v1 types removed in Phase 6: EngineRuntime (class), LayerStack, Layer, Game, old Plugin base class, FpsOverlayLayer, PlatformBackend enum, RenderBackend enum, and the abstract Window/Input/Time/RenderDevice base classes with their factory methods. Phase 7 becomes an audit pass for dead includes, forward declarations, and vestigial references only.
- **D-12:** Platform implementation files (SDL3Window, SDL3Input, etc.) are refactored into the new subsystem implementations (SDLWindowSubsystem, etc.). The SDL-specific logic is preserved but no longer hidden behind an abstract interface -- the subsystem IS the implementation.

### Test Migration
- **D-13:** Full audit and rewrite of all test files. Every test is reviewed against v2 types. Tests that reference v1 types (EngineRuntime, Game, LayerStack, old Plugin) are rewritten. Tests that already use v2 types are verified correct.
- **D-14:** New Application integration test added. Boots Application headless (no platform plugins), optionally adds a test `FixedTimeSubsystem` for deterministic ticking, enters a test state, ticks multiple frames, validates the frame sequence (ProcessPending -> events -> update -> render), confirms clean shutdown. No GPU, no SDL dependency.
- **D-15:** No NullPlatformPlugins group. Tests that need subsystems register only what they need (e.g., a test `FixedTimePlugin`). Tests that don't need subsystems boot Application with no platform plugins at all -- the v2 architecture handles absent subsystems via capability-gating.

### Agent's Discretion
- Internal layout of SDLWindowSubsystem, SDLInputSubsystem etc. (how they wrap SDL3 calls directly)
- How Window close event maps to Application quit (RequestTransition to exit state, or direct flag)
- EngineRenderPlugin internal structure (what render features it registers, how render graph is composed)
- Whether abstract platform interfaces (Window, Input, Time) are kept as documentation/concepts or fully removed
- How EventQueue integrates with the new frame loop (same drain pattern or restructured)
- JourneyPlugin::Build() implementation details (which components, systems, states it registers)
- How existing render features adapt to the collapsed subsystem pattern
- How Application::Loop() handles absent subsystems (no window = no ShouldClose() check, etc.)

### Folded Todos
None.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Architecture Specifications
- `docs/plans/application_architecture_v2.md` -- V2 frame loop design (Application::Loop section), startup lifecycle, subsystem scoping, plugin composition, EngineContext service access pattern
- `docs/plans/application_migration_v2.md` -- Exact rename/keep/remove/add transition tables for EngineRuntime -> subsystems, Game -> Simulation, LayerStack -> OverlayStack, Plugin -> IPlugin
- `docs/plans/game_framework.md` -- Simulation design, GameplayState wrapping Simulation, ServiceProvider access, boot scene loading

### Existing V2 Types (to wire together)
- `engine/wayfinder/src/app/Application.h` + `Application.cpp` -- Current mixed v1/v2 frame loop. V2 members (AppBuilder, AppDescriptor) exist alongside v1 (EngineRuntime, LayerStack, Game). Rewrite target.
- `engine/wayfinder/src/app/ApplicationStateMachine.h` + `.cpp` -- Phase 4 type_index-keyed ASM with deferred transitions. Application owns this.
- `engine/wayfinder/src/app/OverlayStack.h` + `.cpp` -- Phase 4 capability-gated overlay execution. Application owns this.
- `engine/wayfinder/src/app/EngineContext.h` + `.cpp` -- Phase 2/4 service-access facade. Wires ASM, overlays, subsystem registries.
- `engine/wayfinder/src/app/SubsystemRegistry.h` + `SubsystemManifest.h` -- Phase 2/3 subsystem lifecycle management. App-scoped and state-scoped registries.
- `engine/wayfinder/src/app/AppBuilder.h` + `.cpp` -- Phase 3 plugin composition. Finalise() produces AppDescriptor.
- `engine/wayfinder/src/app/AppDescriptor.h` -- Immutable snapshot from AppBuilder::Finalise().
- `engine/wayfinder/src/app/ConfigService.h` -- Phase 3 config service. Per-plugin TOML config.
- `engine/wayfinder/src/app/LifecycleHooks.h` -- Phase 3 OnAppReady, OnStateEnter/Exit hooks.
- `engine/wayfinder/src/gameplay/Simulation.h` + `.cpp` -- Phase 4 StateSubsystem. Thin: flecs world + Scene.
- `engine/wayfinder/src/gameplay/GameplayState.h` + `.cpp` -- Phase 5 IApplicationState wrapping Simulation.
- `engine/wayfinder/src/gameplay/EditorState.h` + `.cpp` -- Phase 5 IApplicationState stub.
- `engine/wayfinder/src/app/PerformanceOverlay.h` + `.cpp` -- Phase 5 IOverlay replacing FpsOverlayLayer.
- `engine/wayfinder/src/gameplay/SceneRenderExtractor.h` + `.cpp` -- Phase 5 gameplay-domain extractor filling SceneCanvas.

### V1 Types to Remove
- `engine/wayfinder/src/app/EngineRuntime.h` + `.cpp` -- Monolithic runtime owning all platform services. Remove entirely.
- `engine/wayfinder/src/app/LayerStack.h` + `.cpp` -- V1 layer system. Remove entirely.
- `engine/wayfinder/src/app/Layer.h` -- V1 layer interface. Remove entirely.
- `engine/wayfinder/src/app/FpsOverlayLayer.h` + `.cpp` -- V1 FPS overlay. Remove entirely.
- `engine/wayfinder/src/gameplay/Game.h` + `.cpp` -- V1 game runtime. Remove entirely.
- `engine/wayfinder/src/platform/BackendConfig.h` -- PlatformBackend/RenderBackend enums. Remove entirely.
- `engine/wayfinder/src/platform/Window.h` -- Abstract Window interface with factory. Remove (SDL-specific code moves into SDLWindowSubsystem).
- `engine/wayfinder/src/platform/Input.h` -- Abstract Input interface with factory. Remove (code moves into SDLInputSubsystem).
- `engine/wayfinder/src/platform/Time.h` -- Abstract Time interface with factory. Remove (code moves into SDLTimeSubsystem).

### V1 Subsystem Wrappers to Collapse
- `engine/wayfinder/src/app/WindowSubsystem.h` + `.cpp` -- Thin wrapper. Collapse into SDLWindowSubsystem.
- `engine/wayfinder/src/app/InputSubsystem.h` + `.cpp` -- Thin wrapper. Collapse into SDLInputSubsystem.
- `engine/wayfinder/src/app/TimeSubsystem.h` + `.cpp` -- Thin wrapper. Collapse into SDLTimeSubsystem.
- `engine/wayfinder/src/app/RenderDeviceSubsystem.h` + `.cpp` -- Thin wrapper. Collapse into SDLRenderDeviceSubsystem.
- `engine/wayfinder/src/app/RendererSubsystem.h` + `.cpp` -- May keep or collapse depending on how tightly it wraps Renderer.

### Platform Implementations (to refactor into subsystems)
- `engine/wayfinder/src/platform/sdl3/SDL3Window.h` + `.cpp` -- SDL3 window impl. Core logic moves into SDLWindowSubsystem.
- `engine/wayfinder/src/platform/sdl3/SDL3Input.h` + `.cpp` -- SDL3 input impl. Core logic moves into SDLInputSubsystem.
- `engine/wayfinder/src/platform/sdl3/SDL3Time.h` + `.cpp` -- SDL3 time impl. Core logic moves into SDLTimeSubsystem.
- `engine/wayfinder/src/platform/null/NullWindow.h` -- Remove entirely. Headless = no window subsystem registered.
- `engine/wayfinder/src/platform/null/NullInput.h` -- Remove entirely. Headless = no input subsystem registered.
- `engine/wayfinder/src/platform/null/NullTime.h` -- Deterministic tick logic moves to test infrastructure as `FixedTimeSubsystem`. Engine file removed.
- `engine/wayfinder/src/rendering/backend/sdl_gpu/SDLGPUDevice.h` + `.cpp` -- SDL_GPU device impl. Core logic moves into SDLRenderDeviceSubsystem.

### Test Files (to audit and rewrite)
- `tests/TestHelpers.h` -- Test utility functions. May need new helpers for v2 boot.
- `tests/app/` -- All app test files: ApplicationStateMachineTests, OverlayStackTests, PerformanceOverlayTests, OrchestrationIntegrationTests, EngineRuntimeTests, EngineSubsystemTests, SubsystemTests, EngineConfigTests, GameplayStateTests, EditorStateTests.
- `tests/gameplay/` -- SimulationTests and other gameplay tests.
- `sandbox/journey/src/JourneyGame.cpp` -- Journey sandbox entry point. Complete rewrite.

### Prior Phase Contexts
- `.planning/phases/01-foundation-types/` -- V2 interfaces, StateMachine, Capability
- `.planning/phases/02-subsystem-infrastructure/` -- SubsystemRegistry, EngineContext, capability activation
- `.planning/phases/03-plugin-composition/` -- AppBuilder, AppDescriptor, ConfigService, PluginGroup
- `.planning/phases/04-orchestration/` -- ASM, OverlayStack, Simulation, lifecycle hooks
- `.planning/phases/05-concrete-states-and-engine-decomposition/` -- Canvas types, AppSubsystems, GameplayState, EditorState, PerformanceOverlay, SceneRenderExtractor

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `ApplicationStateMachine` (Phase 4) -- owns states, deferred transitions, graph validation. Application wires to frame loop.
- `OverlayStack` (Phase 4) -- capability-gated execution view. Application wires events/update/render calls.
- `EngineContext` (Phase 2/4) -- service facade. Wires ASM, overlays, subsystem registries. Central to v2 access pattern.
- `AppBuilder` (Phase 3) -- plugin composition. Finalise() produces AppDescriptor. Application drives this during init.
- `ConfigService` (Phase 3) -- TOML config loading. Subsystems and states read config from here.
- `Simulation` (Phase 4) -- thin StateSubsystem. GameplayState wraps it.
- `GameplayState` (Phase 5) -- IApplicationState wrapping Simulation lifecycle.
- `EditorState` (Phase 5) -- stub IApplicationState with ImGui docking skeleton.
- `PerformanceOverlay` (Phase 5) -- IOverlay rendering frame timing via UICanvas.
- `SceneRenderExtractor` (Phase 5) -- gameplay-domain extractor filling SceneCanvas from ECS.
- `PluginGroup` (Phase 3) -- compose multiple plugins into single AddPlugin call. Used for SDLPlatformPlugins.
- Canvas types (Phase 5) -- SceneCanvas, UICanvas, DebugCanvas, FrameCanvases for render submission.
- `SubsystemRegistry` with DependsOn ordering (Phase 2) -- subsystems declare dependencies, init in topological order.

### Established Patterns
- Plugin registration: `builder.RegisterAppSubsystem<T>(descriptor)` with DependsOn and RequiredCapabilities
- State registration: `builder.AddState<T>()` with AddTransition<From, To>() and AllowPush<T>()
- Overlay registration: `builder.RegisterOverlay<T>(descriptor)` with RequiredCapabilities
- Config loading: `builder.LoadConfig<T>("section")` reads from TOML files with 3-tier layering
- Lifecycle hooks: OnAppReady, OnStateEnter<T>, OnStateExit<T>, OnShutdown lambdas
- Subsystem access: `context.GetAppSubsystem<T>()` / `context.GetStateSubsystem<T>()`

### Integration Points
- `Application::Initialise()` -- must be rewritten to use AppBuilder exclusively, create ASM + OverlayStack, wire EngineContext, drop EngineRuntime/LayerStack/Game
- `Application::Loop()` -- must be rewritten to v2 frame sequence using ASM + OverlayStack + subsystem calls
- `Application::Shutdown()` -- must tear down v2 components (ASM, OverlayStack, subsystem registries)
- `Application::OnEvent()` -- must route SDL events through v2 path (OverlayStack -> ActiveState)
- `sandbox/journey/src/JourneyGame.cpp` -- complete rewrite to `main()` with `AddPlugin<T>()` calls
- `engine/wayfinder/CMakeLists.txt` -- source file list must be updated (new subsystem files, removed v1 files)
- `tests/CMakeLists.txt` -- test file list may change if test files are renamed or added

</code_context>

<specifics>
## Specific Ideas

- Platform subsystem collapse is the key architectural improvement: remove the abstract-interface-plus-enum-factory indirection entirely. SDLWindowSubsystem IS the window, not a wrapper around one.
- Plugin group naming: `SDLPlatformPlugins` (plural) to be explicit that it's a group expanding to multiple plugins.
- Future build-time stripping (CMake options per platform backend) is enabled by the plugin file decomposition but not implemented in Phase 6.
- Phase 7 scope reduced to an audit pass: dead includes, forward declarations, vestigial references. All major v1 type removal happens in Phase 6.

</specifics>

<deferred>
## Deferred Ideas

- **Build-time platform stripping:** CMake options to conditionally exclude SDL3/SDL_GPU code from binary (`WAYFINDER_ENABLE_SDL3`, `WAYFINDER_ENABLE_SDL_GPU`). Plugin file decomposition in Phase 6 makes this trivial to add later.
- **Alternative render backends:** bgfx, wgpu, or other GPU abstraction layers as alternative RenderDeviceSubsystem implementations. Architecture supports this once PlatformBackend enum is removed.
- **Platform module boundaries:** Engine as a set of linkable CMake targets (wayfinder::core, wayfinder::sdl3_platform, wayfinder::sdl_gpu) for finer-grained dependency control.

</deferred>

---

*Phase: 06-application-rewrite-and-integration*
*Context gathered: 2026-04-05*
