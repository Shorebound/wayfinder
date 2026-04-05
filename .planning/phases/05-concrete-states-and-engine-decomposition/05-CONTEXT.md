# Phase 5: Concrete States and Engine Decomposition - Context

**Gathered:** 2026-04-05
**Status:** Ready for planning

<domain>
## Phase Boundary

V2 architecture handles real workloads. GameplayState wraps Simulation in the IApplicationState lifecycle and runs flecs world updates through the v2 frame path. EditorState stub enters and exits cleanly with an ImGui docking skeleton. EngineRuntime is decomposed into five independent AppSubsystems (Window, Input, Time, RenderDevice, Renderer) with proper RAII lifecycle and dependency ordering. Render submission uses typed canvas collectors (SceneCanvas, UICanvas, DebugCanvas) with per-frame buffer reuse. PerformanceOverlay (renamed from FpsOverlay) renders frame timing data via ImGui through UICanvas.

**Requirements:** STATE-06, STATE-07, OVER-05, REND-01, REND-02, REND-03

</domain>

<decisions>
## Implementation Decisions

### GameplayState Design
- **D-01:** GameplayState wraps Simulation minimally. OnEnter creates Simulation as a state subsystem. OnExit destroys it. OnUpdate delegates to Simulation::Update(). OnRender performs extraction and fills the SceneCanvas. Other Game responsibilities (TagRegistry, GameStateMachine, SubsystemCollection) stay in their existing homes or get reworked in Phase 6.
- **D-02:** Boot scene loading flows through ConfigService. The gameplay plugin registers config via `builder.LoadConfig<SimulationConfig>("simulation")`. Simulation reads the boot scene path from ConfigService during Initialise(). TOML file at `config/simulation.toml`. ProjectDescriptor's boot scene becomes the fallback/default when no TOML override exists. Consistent with Phase 3 config architecture.
- **D-03:** ECS singletons (ActiveGameState, SceneSettings, ActiveTags) are set up by Simulation::Initialise() since Simulation owns the flecs world. GameplayState does not touch ECS directly.

### EditorState Stub
- **D-04:** EditorState stub with ImGui docking skeleton. Proves IApplicationState lifecycle for a non-gameplay state. Includes basic ImGui docking layout setup (DockSpaceOverViewport) to provide a foundation for future editor work. No simulation, minimal state subsystems.

### EngineRuntime Decomposition
- **D-05:** Five AppSubsystems: WindowSubsystem, InputSubsystem, TimeSubsystem, RenderDeviceSubsystem, RendererSubsystem. Dependency chain: RendererSubsystem depends on RenderDeviceSubsystem, RenderDeviceSubsystem depends on WindowSubsystem. InputSubsystem and TimeSubsystem are independent leaf nodes.
- **D-06:** RenderDevice is a SEPARATE AppSubsystem from Renderer. Allows future subsystems (compute, asset GPU upload) to depend on the device without pulling in the full renderer. AssetService can depend on RenderDeviceSubsystem directly.
- **D-07:** RendererSubsystem owns Renderer, SceneRenderExtractor integration (via canvas consumption), and BlendableEffectRegistry. BlendableEffectRegistry is rendering-specific -- not a pattern that replicates for audio or other domains. The global singleton pattern (SetActiveInstance) is eliminated.
- **D-08:** Headless support via capability-gating. Window, RenderDevice, and Renderer require Presentation/Rendering capabilities. In headless mode those caps are absent -- subsystems don't activate. Input and Time are always active (empty RequiredCapabilities). Uses the existing Phase 2 capability-gated activation system.

### Canvas Render Submission
- **D-09:** Three typed canvases: SceneCanvas (meshes, lights, camera, environment), UICanvas (ImGui draw lists, 2D commands), DebugCanvas (wireframe lines, shapes, gizmos). Each has fundamentally different data shapes. Canvases contain only rendering vocabulary types -- zero ECS/flecs dependency.
- **D-10:** Renderer-owned, per-frame reset with buffer reuse. RendererSubsystem owns a FrameCanvases member. BeginFrame() calls Reset() which clears entries but keeps allocated memory (zero allocation after warmup). States and overlays write into canvases during their OnRender calls. Renderer processes filled canvases after all OnRender calls complete.
- **D-11:** Canvas-to-render-feature mapping. SceneCanvas feeds scene render features (opaque, shadows, transparency, post-process). UICanvas feeds the UI render feature (final composite layer). DebugCanvas feeds the debug render feature (wireframe overlay). Each canvas type has defined consumers in the render graph via RenderPhase ordering.
- **D-12:** Designed for future persistence upgrade. Canvas interface uses Clear() + Submit() pattern. Swapping from per-frame reset to persistent dirty-tracking is an internal implementation change behind the same interface. Not implemented now -- per-frame extraction is not the bottleneck for target scene density.
- **D-13:** Multi-viewport ready. FrameCanvases can hold vector<SceneCanvas> for N viewports (editor split views, PIE, standalone window). Each viewport gets its own camera and target. Renderer processes all scene canvases. Not implemented in Phase 5 (single viewport), but canvas model doesn't need structural changes for multi-viewport.

### SceneRenderExtractor Scope
- **D-14:** SceneRenderExtractor lives in the gameplay domain. It reads ECS components (Transform, MeshComponent, MaterialComponent, LightComponent) and fills SceneCanvas with render-only primitives. The Renderer never imports ECS types. This is the abstraction boundary between gameplay and rendering.
- **D-15:** SceneRenderExtractor is a standalone utility class with cached flecs queries. NOT a flecs system. Called explicitly by GameplayState::OnRender(). Reasons: extraction timing must be after simulation update but before render graph build (OnRender phase, not OnUpdate phase); non-ECS states (EditorState) need a different extraction path; decoupled from flecs pipeline scheduler for future parallel extraction.
- **D-16:** For WoW++-density scenes (50K+ renderable entities), the extraction cost is dominated by ECS iteration (~1-2ms), not canvas filling. Frustum culling (spatial indexing pre-cull) and GPU-driven rendering (instancing, indirect draw) are the real performance wins -- both operate downstream of the canvas model and are supported cleanly by it.

### PerformanceOverlay (renamed from FpsOverlay)
- **D-17:** Named PerformanceOverlay from the start. Starts minimal (FPS + frame time), structure supports growth to GPU timing, draw calls, memory without renaming.
- **D-18:** Hybrid data source. TimeSubsystem provides raw delta time (already available via GetDeltaTime()). PerformanceOverlay does its own display-optimised averaging (e.g., update 4x/second) for human-readable output.
- **D-19:** ImGui via UICanvas. First consumer of ImGui through the overlay system. ImGui draw lists submit through UICanvas. The UI render feature consumes ImGui draw data.
- **D-20:** Requires Presentation capability. Active whenever a window exists.
- **D-21:** Registered but disabled in Shipping builds. Runtime toggle via ActivateOverlay/DeactivateOverlay. Developer consoles or flags can enable it. Zero visual cost when inactive (OnRender short-circuits).

### Agent's Discretion
- FrameCanvases struct layout and how canvases are accessed by states/overlays (direct member access vs getter methods)
- SceneCanvas internal data structures (vector of MeshSubmission, separate arrays per type, SoA vs AoS)
- UICanvas ImGui integration details (how ImDrawList data is captured and stored)
- DebugCanvas primitive types and how they map to the debug render feature
- SimulationConfig struct fields beyond boot scene (tick rate, sub-stepping, etc.)
- EditorState ImGui docking layout specifics (which panels are stubbed)
- How BlendableEffectRegistry is accessed within RendererSubsystem (replacing global singleton)
- RenderDeviceSubsystem init sequence and what it exposes beyond the raw device
- WindowSubsystem API surface (how it wraps the current Window + provides ShouldClose, events, etc.)
- Whether InputSubsystem wraps the existing Input class directly or reimplements
- How existing render features (SceneOpaquePass etc.) are adapted to read from SceneCanvas instead of RenderFrame

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Architecture Specifications
- `docs/plans/application_architecture_v2.md` -- EngineRuntime decomposition (Subsystem Scoping section), canvas-based render submission design, startup lifecycle, subsystem dependency ordering
- `docs/plans/application_migration_v2.md` -- EngineRuntime -> individual subsystems transition table, Game -> Simulation migration path, RenderFrame evolution
- `docs/plans/game_framework.md` -- Simulation design, GameplayState wrapping Simulation, ServiceProvider access pattern, Scene ownership model, ActiveGameState singleton wiring
- `docs/plans/rendering_performance.md` -- Render performance targets and strategy (if relevant to canvas design)

### Existing Types (to build on or evolve)
- `engine/wayfinder/src/gameplay/Game.h` + `Game.cpp` -- Current v1 runtime to study. What GameplayState replaces for ECS/scene lifecycle. Note: TagRegistry, GameStateMachine, SubsystemCollection stay outside Simulation scope.
- `engine/wayfinder/src/gameplay/Simulation.h` -- Phase 4 v2 StateSubsystem. Thin: owns flecs::world + Scene. GameplayState creates this in OnEnter.
- `engine/wayfinder/src/app/IApplicationState.h` -- Phase 4 interface with BackgroundMode negotiation. GameplayState and EditorState implement this.
- `engine/wayfinder/src/app/ApplicationStateMachine.h` -- Phase 4 type_index-keyed ASM. Manages GameplayState/EditorState lifecycle.
- `engine/wayfinder/src/app/EngineRuntime.h` + `EngineRuntime.cpp` -- Monolithic runtime to decompose. Study ownership: Window, Input, Time, RenderDevice, Renderer, SceneRenderExtractor, BlendableEffectRegistry. Note init dependency chain.
- `engine/wayfinder/src/app/Application.h` + `Application.cpp` -- Current frame loop, EngineRuntime usage, LayerStack push of FpsOverlayLayer. Frame loop structure must be adapted for canvas submission.
- `engine/wayfinder/src/app/FpsOverlayLayer.h` -- V1 Layer-based FPS display. Study averaging logic for PerformanceOverlay migration.
- `engine/wayfinder/src/app/IOverlay.h` -- Phase 4 v2 interface. PerformanceOverlay implements this.
- `engine/wayfinder/src/app/OverlayStack.h` -- Phase 4 capability-gated execution view. PerformanceOverlay registered and managed here.
- `engine/wayfinder/src/rendering/pipeline/Renderer.h` + `Renderer.cpp` -- Current render pipeline. Must be adapted to consume canvases instead of flat RenderFrame.
- `engine/wayfinder/src/rendering/pipeline/SceneRenderExtractor.h` -- Current extraction logic. Moves to gameplay domain, adapted to fill SceneCanvas from ECS queries.
- `engine/wayfinder/src/rendering/graph/RenderFrame.h` -- Current flat submission structs. SceneCanvas replaces/evolves this as the producer-side interface.
- `engine/wayfinder/src/rendering/graph/RenderFeature.h` -- Base class with SetEnabled(). Scene features adapted to read from SceneCanvas.
- `engine/wayfinder/src/rendering/pipeline/RenderOrchestrator.h` -- RenderPhase enum, feature registration and graph building. Canvas-to-feature routing happens here.
- `engine/wayfinder/src/app/SubsystemRegistry.h` + `SubsystemManifest.h` -- Registration and dependency ordering for the five new AppSubsystems.
- `engine/wayfinder/src/app/EngineContext.h` -- Service access facade. New subsystems accessed via GetAppSubsystem<WindowSubsystem>() etc.
- `engine/wayfinder/src/app/AppBuilder.h` -- Plugin registration. New subsystems registered via plugins.
- `engine/wayfinder/src/gameplay/Capability.h` -- CapabilitySet for headless gating (Presentation, Rendering).
- `engine/wayfinder/src/app/ConfigService.h` -- Phase 3 config service. SimulationConfig loaded via builder.LoadConfig<SimulationConfig>("simulation").

### Prior Phase Contexts
- `.planning/phases/01-foundation-types/` -- IApplicationState, IOverlay, StateMachine, Capability foundations
- `.planning/phases/02-subsystem-infrastructure/` -- SubsystemRegistry, EngineContext, SubsystemManifest, capability-gated activation, AppSubsystem lifecycle
- `.planning/phases/03-plugin-composition/` -- AppBuilder, AppDescriptor, ConfigService, processed-output pattern
- `.planning/phases/04-orchestration/` -- ApplicationStateMachine, OverlayStack, Simulation as StateSubsystem, BackgroundMode negotiation, IStateUI lifecycle

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `Simulation` (Phase 4 StateSubsystem) -- GameplayState wraps this directly, no new simulation code needed
- `OverlayStack` (Phase 4) -- PerformanceOverlay registers through existing builder.RegisterOverlay<T>() path
- `ApplicationStateMachine` (Phase 4) -- GameplayState and EditorState register via builder.AddState<T>()
- `RenderFeature::SetEnabled()` -- already exists on the base class, REND-03 capability-gating builds on this
- `SubsystemRegistry` with DependsOn ordering (Phase 2) -- five new AppSubsystems use existing registration infra
- `ConfigService` (Phase 3) -- SimulationConfig loaded through existing builder.LoadConfig<T>() path
- ImGui already in thirdparty/ -- PerformanceOverlay is first consumer via UICanvas

### Established Patterns
- Subsystem registration: `Register<T>(SubsystemDescriptor{.DependsOn = Deps<X, Y>()})` with Result<void> Initialise
- State registration: `builder.AddState<T>()` with typed transitions via AddTransition<From, To>()
- Overlay registration: `builder.RegisterOverlay<T>()` with capability requirements
- Plugin config: `builder.LoadConfig<T>("section")` maps to `config/section.toml`
- Render feature: `AddFeature(RenderPhase, order, unique_ptr<RenderFeature>)` with `AddPasses(RenderGraph&, FrameRenderParams&)`

### Integration Points
- Application::Loop() needs canvas BeginFrame/Render calls wired into frame sequence
- EngineRuntime creation in Application::Initialise() replaced by five subsystem registrations
- FpsOverlayLayer push in Application::Initialise() replaced by PerformanceOverlay registration in plugin
- SceneRenderExtractor moves from EngineRuntime to gameplay domain (GameplayState member)
- RenderFrame consumption in Renderer::Render() adapted to canvas input

</code_context>

<specifics>
## Specific Ideas

- WoW++-density scenes as the performance target. Architecture must support 50K+ renderable entities with frustum culling and future GPU-driven rendering downstream of canvases.
- EditorState should have ImGui docking skeleton, not just empty enter/exit. Foundation for future editor work.
- PerformanceOverlay is the first ImGui consumer through the overlay/canvas pipeline. Establishes the pattern for all future ImGui overlays.
- Multi-viewport (editor split views, PIE) is a stated goal. Canvas model with vector<SceneCanvas> supports this without structural changes.
- Extraction-as-utility (not flecs system): explicit calling from GameplayState::OnRender preferred over ECS pipeline integration. Decouples rendering timing from simulation timing.

</specifics>

<deferred>
## Deferred Ideas

- Persistent canvas with dirty-tracking (swap Clear() implementation if per-frame extraction becomes a bottleneck)
- Multi-viewport implementation (canvas model supports it, actual implementation deferred)
- GPU-driven rendering / indirect draw calls (downstream of canvas model)
- Full editor implementation (EditorState stub only in this phase)
- FpsOverlay evolution to full performance metrics (GPU timing, draw calls, memory -- as those systems are built)
- SceneRenderExtractor as flecs system in PreRender phase (alternative model if flecs ever owns the full frame loop)

</deferred>

---

*Phase: 05-concrete-states-and-engine-decomposition*
*Context gathered: 2026-04-05*
