# Phase 5: Concrete States and Engine Decomposition - Research

**Researched:** 2026-04-05
**Domain:** Application state lifecycle, engine runtime decomposition, canvas-based render submission, C++23 patterns
**Confidence:** HIGH

## Summary

Phase 5 bridges the v2 infrastructure (Phases 1-4) to real workloads. It transforms the monolithic `EngineRuntime` into five independent `AppSubsystem` instances, implements `GameplayState` and `EditorState` as concrete `IApplicationState` consumers, introduces a typed canvas submission model that decouples gameplay from rendering, and routes ImGui through the overlay system via `PerformanceOverlay`.

The existing codebase provides substantial infrastructure: `SubsystemRegistry` with topological ordering, `ApplicationStateMachine` with push/pop, `OverlayStack` with capability gating, `Simulation` as a thin `StateSubsystem`, `ConfigService` for TOML loading, and `RenderFeature::SetEnabled()` for dynamic feature gating. Phase 5 is predominantly a wiring and decomposition phase - creating new types that compose existing primitives rather than inventing new infrastructure.

**Primary recommendation:** Decompose EngineRuntime bottom-up (Window -> RenderDevice -> Renderer), introduce canvas types as pure data structures with no ECS dependency, move SceneRenderExtractor to the gameplay domain as a utility called by GameplayState::OnRender, and implement PerformanceOverlay as the first ImGui-through-UICanvas consumer.

<user_constraints>

## User Constraints (from CONTEXT.md)

### Locked Decisions

**GameplayState Design:**
- D-01: GameplayState wraps Simulation minimally. OnEnter creates Simulation as a state subsystem. OnExit destroys it. OnUpdate delegates to Simulation::Update(). OnRender performs extraction and fills the SceneCanvas. Other Game responsibilities (TagRegistry, GameStateMachine, SubsystemCollection) stay in their existing homes or get reworked in Phase 6.
- D-02: Boot scene loading flows through ConfigService. The gameplay plugin registers config via `builder.LoadConfig<SimulationConfig>("simulation")`. Simulation reads the boot scene path from ConfigService during Initialise(). TOML file at `config/simulation.toml`. ProjectDescriptor's boot scene becomes the fallback/default when no TOML override exists.
- D-03: ECS singletons (ActiveGameState, SceneSettings, ActiveTags) are set up by Simulation::Initialise() since Simulation owns the flecs world. GameplayState does not touch ECS directly.

**EditorState Stub:**
- D-04: EditorState stub with ImGui docking skeleton. Proves IApplicationState lifecycle for a non-gameplay state. Includes basic ImGui docking layout setup (DockSpaceOverViewport) to provide a foundation for future editor work. No simulation, minimal state subsystems.

**EngineRuntime Decomposition:**
- D-05: Five AppSubsystems: WindowSubsystem, InputSubsystem, TimeSubsystem, RenderDeviceSubsystem, RendererSubsystem. Dependency chain: RendererSubsystem depends on RenderDeviceSubsystem, RenderDeviceSubsystem depends on WindowSubsystem. InputSubsystem and TimeSubsystem are independent leaf nodes.
- D-06: RenderDevice is a SEPARATE AppSubsystem from Renderer. Allows future subsystems (compute, asset GPU upload) to depend on the device without pulling in the full renderer. AssetService can depend on RenderDeviceSubsystem directly.
- D-07: RendererSubsystem owns Renderer, SceneRenderExtractor integration (via canvas consumption), and BlendableEffectRegistry. BlendableEffectRegistry is rendering-specific. The global singleton pattern (SetActiveInstance) is eliminated.
- D-08: Headless support via capability-gating. Window, RenderDevice, and Renderer require Presentation/Rendering capabilities. In headless mode those caps are absent - subsystems don't activate. Input and Time are always active (empty RequiredCapabilities). Uses the existing Phase 2 capability-gated activation system.

**Canvas Render Submission:**
- D-09: Three typed canvases: SceneCanvas (meshes, lights, camera, environment), UICanvas (ImGui draw lists, 2D commands), DebugCanvas (wireframe lines, shapes, gizmos). Each has fundamentally different data shapes. Canvases contain only rendering vocabulary types - zero ECS/flecs dependency.
- D-10: Renderer-owned, per-frame reset with buffer reuse. RendererSubsystem owns a FrameCanvases member. BeginFrame() calls Reset() which clears entries but keeps allocated memory (zero allocation after warmup). States and overlays write into canvases during their OnRender calls. Renderer processes filled canvases after all OnRender calls complete.
- D-11: Canvas-to-render-feature mapping. SceneCanvas feeds scene render features (opaque, shadows, transparency, post-process). UICanvas feeds the UI render feature (final composite layer). DebugCanvas feeds the debug render feature (wireframe overlay). Each canvas type has defined consumers in the render graph via RenderPhase ordering.
- D-12: Designed for future persistence upgrade. Canvas interface uses Clear() + Submit() pattern. Swapping from per-frame reset to persistent dirty-tracking is an internal implementation change behind the same interface.
- D-13: Multi-viewport ready. FrameCanvases can hold vector<SceneCanvas> for N viewports. Not implemented in Phase 5 (single viewport), but canvas model doesn't need structural changes.

**SceneRenderExtractor Scope:**
- D-14: SceneRenderExtractor lives in the gameplay domain. It reads ECS components and fills SceneCanvas with render-only primitives. The Renderer never imports ECS types. This is the abstraction boundary between gameplay and rendering.
- D-15: SceneRenderExtractor is a standalone utility class with cached flecs queries. NOT a flecs system. Called explicitly by GameplayState::OnRender().
- D-16: For WoW++-density scenes (50K+), extraction cost is ECS iteration (~1-2ms). Frustum culling and GPU-driven rendering operate downstream of canvas model.

**PerformanceOverlay:**
- D-17: Named PerformanceOverlay from the start. Starts minimal (FPS + frame time).
- D-18: Hybrid data source. TimeSubsystem provides raw delta time. PerformanceOverlay does its own display-optimised averaging.
- D-19: ImGui via UICanvas. First consumer of ImGui through the overlay system.
- D-20: Requires Presentation capability. Active whenever a window exists.
- D-21: Registered but disabled in Shipping builds. Runtime toggle via ActivateOverlay/DeactivateOverlay.

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

### Deferred Ideas (OUT OF SCOPE)

- Persistent canvas with dirty-tracking
- Multi-viewport implementation
- GPU-driven rendering / indirect draw calls
- Full editor implementation
- FpsOverlay evolution to full performance metrics (GPU timing, draw calls, memory)
- SceneRenderExtractor as flecs system in PreRender phase

</user_constraints>

<phase_requirements>

## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| STATE-06 | GameplayState wrapping Simulation into IApplicationState lifecycle | GameplayState pattern, Simulation as StateSubsystem, OnEnter/OnExit/OnUpdate/OnRender lifecycle, SceneRenderExtractor called from OnRender |
| STATE-07 | EditorState stub proving the IApplicationState pattern | EditorState with ImGui DockSpaceOverViewport skeleton, empty lifecycle, no simulation |
| OVER-05 | FpsOverlay rewritten from FpsOverlayLayer | PerformanceOverlay as IOverlay, ImGui rendering via UICanvas, averaging logic migrated from FpsOverlayLayer |
| REND-01 | EngineRuntime decomposed into individual AppSubsystems (Window, Input, Time, Renderer) | Five AppSubsystem types with dependency ordering and capability gating via SubsystemRegistry |
| REND-02 | Canvas-based render submission model (typed per-frame data collectors) | SceneCanvas, UICanvas, DebugCanvas types; FrameCanvases aggregate; per-frame reset with buffer reuse |
| REND-03 | Render features with capability-gated activation via SetEnabled() | Existing RenderFeature::SetEnabled() used during capability transitions, render features read from canvas types |

</phase_requirements>

## Project Constraints (from copilot-instructions.md)

Key directives affecting this phase:

- **C++23 features actively used:** `std::expected` (via Result<T>), deducing `this`, concepts/requires, structured bindings, `std::generator<T>` where useful, `auto(x)` decay-copy
- **RAII everywhere.** Every subsystem wraps its resource; destructor releases it
- **Result<void> for recoverable failures.** Subsystem Initialise must return Result<void>
- **Trailing return types.** `auto Foo() -> ReturnType`
- **West const.** `const T`, never east const
- **`and`/`or`/`not` over `&&`/`||`/`!`**
- **`[[nodiscard]]`** on functions returning resources, handles, or values the caller must inspect
- **British spelling.** `Initialise`, `Colour`, `Serialise`, `Behaviour`
- **Naming conventions.** PascalCase types/functions, `m_` prefix for private members, `I` prefix for interfaces
- **No em-dashes** (U+2014) in code, strings, or comments
- **doctest** for testing, headless only, no filesystem outside `tests/fixtures/`
- **Source files listed explicitly** in CMakeLists.txt
- **`Wayfinder::Log` category logging** rather than ad hoc console output
- **Data files:** TOML for hand-authored, JSON for interchange
- **`InternedString`** for stable, repeatedly compared identifiers
- **Engine code lives in `Wayfinder` namespace** with sub-namespaces matching domain

## Architecture Patterns

### Pattern 1: Subsystem Decomposition (EngineRuntime -> Five AppSubsystems)

**What:** Decompose the monolithic `EngineRuntime` into five independently registered `AppSubsystem` types with declared dependencies and capability requirements. The existing `SubsystemRegistry<AppSubsystem>` with topological ordering handles init/shutdown sequence automatically.

**Current EngineRuntime ownership:**
```
EngineRuntime
  +-- m_input       (unique_ptr<Input>)
  +-- m_time        (unique_ptr<Time>)
  +-- m_window      (unique_ptr<Window>)
  +-- m_device      (unique_ptr<RenderDevice>)
  +-- m_renderer    (unique_ptr<Renderer>)
  +-- m_extractor   (unique_ptr<SceneRenderExtractor>)
  +-- m_blendableEffectRegistry (BlendableEffectRegistry, by value)
```

**Target decomposition:**
```
AppSubsystem              DependsOn           RequiredCapabilities
---------                 ---------           --------------------
WindowSubsystem           (none)              { Presentation }
InputSubsystem            (none)              (empty = always active)
TimeSubsystem             (none)              (empty = always active)
RenderDeviceSubsystem     WindowSubsystem     { Rendering }
RendererSubsystem         RenderDeviceSubsystem  { Rendering }
```

**Each subsystem follows the existing pattern:**
```cpp
class WindowSubsystem : public AppSubsystem
{
public:
    [[nodiscard]] auto Initialise(EngineContext& context) -> Result<void> override;
    void Shutdown() override;

    [[nodiscard]] auto GetWindow() -> Window&;
    [[nodiscard]] auto ShouldClose() const -> bool;

private:
    std::unique_ptr<Window> m_window;
};
```

**Registration via plugin (consistent with Phase 3 AppBuilder pattern):**
```cpp
void CoreEnginePlugin::Build(AppBuilder& builder)
{
    builder.GetRegistrar<SubsystemRegistry<AppSubsystem>>()
        .Register<WindowSubsystem>({ .RequiredCapabilities = { Capability::Presentation } });

    builder.GetRegistrar<SubsystemRegistry<AppSubsystem>>()
        .Register<RenderDeviceSubsystem>({
            .RequiredCapabilities = { Capability::Rendering },
            .DependsOn = Deps<WindowSubsystem>(),
        });

    // ...etc
}
```

**How reference engines do it:**
- **RavEngine:** `App` owns `RenderEngine`, `Window`, `InputManager`, `AudioPlayer` as direct members. Simpler but tightly coupled - no headless mode, no independent subsystem lifecycle.
- **Wicked Engine:** `Scene` is self-contained with all component managers. The rendering pipeline reads scene data directly. No explicit subsystem decomposition.
- **Bevy:** Every engine capability is a Plugin (RenderPlugin, WindowPlugin, InputPlugin, TimePlugin). Each plugin adds its own resources, systems, and sub-apps. This is the closest model to Wayfinder's AppSubsystem approach.
- **O3DE:** Uses "System Components" registered in application descriptors. Each system component declares dependencies via `GetRequiredServices()`/`GetProvidedServices()`. Very similar to Wayfinder's SubsystemRegistry model.
- **Unreal Engine 5:** Modules and subsystems. `UGameInstanceSubsystem`, `UWorldSubsystem`, `UEngineSubsystem` with automatic lifecycle tied to their owner's scope.

**Recommendation:** Follow the existing SubsystemRegistry pattern exactly. Each subsystem is a thin RAII wrapper around the existing concrete type (Window, Input, Time, RenderDevice, Renderer). The wrapper adds dependency declaration and capability gating. The concrete types themselves don't change - they're just owned by a subsystem instead of EngineRuntime.

### Pattern 2: Canvas-Based Render Submission

**What:** Typed per-frame data collectors that decouple producers (states, overlays) from the consumer (renderer). Three canvas types with fundamentally different data shapes.

**Industry patterns:**
- **Bevy's Extract Phase:** A separate "render world" receives extracted data from the "main world" each frame. Systems in the `ExtractSchedule` run against `MainWorld` and write to the render world's resources. The render world's data is purely rendering vocabulary - no gameplay types.
- **Unreal Engine's Scene Proxies:** `FPrimitiveSceneProxy` is the render-thread representation of a component. Created on the game thread, owned by the render thread. The proxy contains only rendering data. `FSceneRenderer::Render()` reads proxies, not components.
- **Wicked Engine:** The `Scene` struct contains parallel arrays of render data (`RenderData` member) that are populated during `Update()` and consumed during rendering. Tightly integrated - the scene IS the render submission.
- **RavEngine:** `World::RenderData` holds GPU-friendly sparse sets and MDI command structures. Updated when components change (observer pattern), not per-frame extraction.
- **Unity's SRP:** `ScriptableRenderContext` collects rendering commands. Camera culling results are passed to render passes via `CullingResults`. The render pipeline reads from culling results, not from GameObjects.

**Wayfinder's canvas model sits between Bevy's world extraction and Unreal's proxy system:**
```
GameplayState::OnRender()
    |
    SceneRenderExtractor::Extract(world, sceneCanvas)  -- reads ECS, writes canvas
    |
PerformanceOverlay::OnRender()
    |
    ImGui::Render() -> uiCanvas.SubmitImGuiDrawData()
    |
Renderer::Render(frameCanvases)
    |
    SceneCanvas -> SceneOpaquePass, ShadowPass, TransparentPass, PostProcessPass
    UICanvas -> UIRenderFeature
    DebugCanvas -> DebugRenderFeature
```

**Canvas data shapes:**
```cpp
/// Scene canvas - meshes, lights, cameras, environment settings
struct SceneCanvas
{
    std::vector<RenderView> Views;
    std::vector<RenderMeshSubmission> Meshes;
    std::vector<RenderLightSubmission> Lights;
    RenderDebugDrawList DebugDraw;
    BlendableEffectStack PostProcess;

    void Clear();  // Clears entries, keeps capacity
    void SubmitMesh(RenderMeshSubmission submission);
    void SubmitLight(RenderLightSubmission light);
};
```

The canvas types are essentially a reorganised `RenderFrame` split by domain. The existing `RenderFrame` struct already contains `Views`, `Layers` (with meshes), `Lights`, and `DebugDraw` - the canvas model restructures this into typed submission targets rather than a flat frame bag.

**Key insight:** The canvas types should reuse the existing RenderFrame vocabulary types (`RenderMeshSubmission`, `RenderLightSubmission`, `RenderView`, `RenderDebugDrawList`, etc.) rather than creating new ones. The change is in ownership and flow, not in data.

### Pattern 3: ECS-to-Render Bridge (SceneRenderExtractor)

**What:** A utility that reads ECS components and writes rendering vocabulary into a canvas. The abstraction boundary between gameplay and rendering.

**Industry patterns:**
- **Bevy's Extract systems:** `extract_meshes`, `extract_lights`, `extract_cameras` are separate systems that read from `MainWorld` and insert into render world resources. Each runs independently, enabling parallel extraction.
- **Unreal's FPrimitiveSceneProxy:** Created once per component, updated on change. `GetDynamicMeshElements()` called per frame by the renderer. The proxy IS the bridge.
- **Godot's VisualServer:** Scene nodes push changes via `RenderingServer::instance_set_transform()`. The rendering server maintains its own copy of scene data.

**Wayfinder approach (decided in D-14/D-15):** SceneRenderExtractor is a standalone utility with cached flecs queries, called explicitly by `GameplayState::OnRender()`. This is closer to Bevy's extraction but with explicit call timing instead of scheduler-driven execution.

**Current SceneRenderExtractor reads from Scene (ECS world via flecs):**
```
Scene.GetWorld().each<Transform, MeshComponent>(...)
  -> Builds RenderMeshSubmission
  -> Adds to RenderFrame layers
```

**Target: reads ECS world, writes SceneCanvas:**
```
SceneRenderExtractor.Extract(world, sceneCanvas)
  -> world.each<Transform, MeshComponent>(...)
  -> sceneCanvas.SubmitMesh(RenderMeshSubmission{...})
```

The extraction logic is largely the same - it's a move of the existing code from `rendering/pipeline/` to `gameplay/` with the output changing from `RenderFrame&` to `SceneCanvas&`.

### Pattern 4: ImGui Through Canvas/Overlay System

**What:** ImGui draw data flows through UICanvas rather than being rendered directly. PerformanceOverlay is the first consumer.

**How modern engines handle ImGui:**
- **Oxylus:** ImGui is integrated at the renderer level with a dedicated ImGui render pass. Not routed through an abstraction.
- **Wicked Engine:** ImGui is optional, compiled when `WICKEDENGINE_BUILD_IMGUI` is set. Rendered as a final overlay pass.
- **Spartan:** `ImGuiExtension` class wraps ImGui. Rendered in a dedicated pass after all scene and post-process passes.
- **O3DE:** ImGui integration module. Draws in an overlay pass.

**Common pattern:** ImGui is always rendered as a final overlay, after scene and post-process. The draw data (ImDrawData) is captured once per frame and submitted to a dedicated render pass.

**Wayfinder approach:**
```cpp
class PerformanceOverlay : public IOverlay
{
    void OnRender(EngineContext& ctx) override
    {
        // Update display values
        UpdateAveraging(ctx.GetAppSubsystem<TimeSubsystem>().GetDeltaTime());

        // Render ImGui window
        ImGui::Begin("Performance");
        ImGui::Text("FPS: %.0f", m_displayFps);
        ImGui::Text("Frame: %.2f ms", m_displayMs);
        ImGui::End();
    }
};
```

The UICanvas captures ImGui draw data for the frame:
```cpp
class UICanvas
{
public:
    void CaptureImGuiDrawData();  // Called after all OnRender() calls
    auto GetImDrawData() const -> const ImDrawData*;
    void Clear();

private:
    // ImGui draw data is owned by ImGui context - we just snapshot it
    bool m_hasImGuiData = false;
};
```

The UI render feature consumes this in the Overlay render phase. ImGui's own rendering context manages draw lists - the UICanvas just gates when data is flushed to the renderer.

### Anti-Patterns to Avoid

- **Subsystem wrapping as reimplementation.** WindowSubsystem should own a `Window`, not reimplement window creation. The subsystem adds lifecycle management, not new functionality.
- **Canvas as a general-purpose command buffer.** Canvases are typed data collectors for specific domains. Don't make a generic `RenderCommand` variant type.
- **SceneRenderExtractor importing canvas details.** The extractor writes to SceneCanvas via submit methods. It doesn't know about the renderer, render features, or the render graph.
- **BlendableEffectRegistry global singleton in new code.** D-07 explicitly eliminates `SetActiveInstance()`. The registry lives in RendererSubsystem and is passed by reference where needed.
- **ImGui calls as part of canvas - ImGui IS the canvas.** Don't try to abstract ImGui draw commands into canvas entries. ImGui manages its own draw lists. The UICanvas just captures the `ImDrawData*` at the right time.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Subsystem dependency ordering | Manual init sequences | SubsystemRegistry with Deps<> | Phase 2 already solved this with topological sort and cycle detection |
| Capability-gated activation | if/else chains in init code | SubsystemDescriptor::RequiredCapabilities | Phase 2's uniform activation rules apply to all subsystems |
| ImGui integration | Custom immediate-mode UI renderer | Dear ImGui (already in thirdparty/) | Mature, battle-tested, trivial to integrate |
| ImGui rendering | Custom ImGui backend | Existing render device + a UIRenderFeature | ImGui provides backend-agnostic draw lists |
| State transition management | Manual state lifecycle | ApplicationStateMachine (Phase 4) | Deferred transitions, push/pop, BackgroundMode all handled |
| Config loading | Manual TOML parsing in subsystems | ConfigService + builder.LoadConfig<T>() | Phase 3 pattern: struct defaults -> config TOML -> saved TOML |
| Overlay lifecycle | Manual ImGui overlay management | OverlayStack (Phase 4) | Capability gating, runtime toggle, ordered execution |

## Common Pitfalls

### Pitfall 1: Circular Dependencies in Subsystem Decomposition
**What goes wrong:** RendererSubsystem needs WindowSubsystem for swapchain creation, but WindowSubsystem might need Renderer for event callbacks (resize).
**Why it happens:** Tight coupling in the original EngineRuntime where everything had access to everything.
**How to avoid:** Use the dependency chain from D-05 strictly. Window resize events flow through the event system, not through direct Renderer references. The Renderer receives resize events via Application's event dispatch, not by depending on Window.
**Warning signs:** `Deps<>` vectors that form cycles. SubsystemRegistry::Finalise() will detect these.

### Pitfall 2: SceneRenderExtractor Leaking ECS Types Into Rendering
**What goes wrong:** Canvas types end up importing flecs headers because they contain ECS-specific data (entity IDs, component references).
**Why it happens:** Temptation to include entity IDs for "later correlation" or to pass ECS data directly.
**How to avoid:** Canvas types live in `rendering/` and use only rendering vocabulary types (`RenderMeshSubmission`, `RenderLightSubmission`, etc.). The extractor in `gameplay/` is the ONLY place that touches both domains. Test: `SceneCanvas.h` must compile without any flecs include.
**Warning signs:** flecs includes in any rendering/ header.

### Pitfall 3: FrameCanvases Ownership Confusion
**What goes wrong:** Multiple owners try to reset/clear canvases, or states hold stale pointers across frames.
**Why it happens:** Unclear lifecycle - who calls Reset(), when are canvases valid?
**How to avoid:** RendererSubsystem owns FrameCanvases. BeginFrame() resets all canvases. States and overlays receive canvas references through EngineContext during their OnRender calls. References are valid only within the current frame.
**Warning signs:** Use-after-reset bugs, double-clearing, canvas data appearing from previous frames.

### Pitfall 4: ImGui Context Lifetime vs Overlay Lifecycle
**What goes wrong:** ImGui::NewFrame() / ImGui::Render() called at wrong times relative to overlay OnRender() calls.
**Why it happens:** ImGui has a global context with frame-scoped state. If NewFrame isn't called before overlays render, or Render() happens too early, draw data is corrupted.
**How to avoid:** ImGui::NewFrame() during the frame's begin phase (before state/overlay OnRender). ImGui::Render() after all OnRender calls complete. UICanvas captures the resulting ImDrawData*. The UIRenderFeature draws it.
**Warning signs:** ImGui assertion failures, blank ImGui windows, draw data corruption.

### Pitfall 5: Breaking Existing v1 Code Paths
**What goes wrong:** Phase 5 changes break the existing Application::Loop() which still runs the v1 path.
**Why it happens:** Phase 5 introduces new subsystems but the Application switchover to v2 is Phase 6.
**How to avoid:** New subsystems coexist with EngineRuntime. They can be registered and initialised but Application::Loop() doesn't use them yet. Phase 6 is the switchover. Phase 5 tests exercise the new types independently.
**Warning signs:** Existing tests failing, Journey sandbox breaking.

### Pitfall 6: Over-Abstracting Canvas Types
**What goes wrong:** Creating complex inheritance hierarchies, visitor patterns, or generic canvas templates to handle the three canvas types.
**Why it happens:** Instinct to unify three similar-looking types.
**How to avoid:** D-09 explicitly states each canvas has fundamentally different data shapes. Three concrete structs. No base class. No variant. The renderer knows exactly which canvas types exist and processes them by name.
**Warning signs:** `ICanvas` base class, `std::variant<SceneCanvas, UICanvas, DebugCanvas>`, canvas type enum.

## Code Examples

### AppSubsystem Registration Pattern
```cpp
// Source: existing SubsystemRegistry pattern (engine/wayfinder/src/app/SubsystemRegistry.h)
class WindowSubsystem : public AppSubsystem
{
public:
    [[nodiscard]] auto Initialise(EngineContext& context) -> Result<void> override
    {
        // Read config from ConfigService
        const auto* config = context.TryGetAppSubsystem<ConfigService>();
        auto windowConfig = Window::Config{
            .Width = 1920, .Height = 1080, .Title = "Wayfinder",
        };
        // Override from config if available...

        m_window = Window::Create(windowConfig, PlatformBackend::SDL);
        if (not m_window)
        {
            return MakeError("Failed to create Window");
        }
        return m_window->Initialise();
    }

    void Shutdown() override
    {
        if (m_window)
        {
            m_window->Shutdown();
            m_window.reset();
        }
    }

    [[nodiscard]] auto GetWindow() -> Window& { return *m_window; }
    [[nodiscard]] auto ShouldClose() const -> bool { return m_window and m_window->ShouldClose(); }

private:
    std::unique_ptr<Window> m_window;
};
```

### Canvas Types (Agent's Discretion Recommendation)
```cpp
// SceneCanvas - separate arrays per submission type (SoA-style for cache locality)
struct SceneCanvas
{
    std::vector<RenderView> Views;
    std::vector<RenderMeshSubmission> Meshes;
    std::vector<RenderLightSubmission> Lights;
    RenderDebugDrawList DebugDraw;
    BlendableEffectStack PostProcess;

    void Clear()
    {
        Views.clear();
        Meshes.clear();
        Lights.clear();
        DebugDraw = {};
        PostProcess = {};
    }

    auto AddView(RenderView view) -> size_t
    {
        Views.push_back(std::move(view));
        return Views.size() - 1;
    }

    void SubmitMesh(RenderMeshSubmission submission) { Meshes.push_back(std::move(submission)); }
    void SubmitLight(RenderLightSubmission light) { Lights.push_back(std::move(light)); }
};

// UICanvas - ImGui draw data capture
struct UICanvas
{
    void Clear() { m_hasImGuiData = false; }

    /// Snapshot ImGui's current draw data after all overlays have rendered
    void CaptureImGuiDrawData() { m_hasImGuiData = true; }

    [[nodiscard]] auto HasImGuiData() const -> bool { return m_hasImGuiData; }

private:
    bool m_hasImGuiData = false;
};

// DebugCanvas - wireframe primitives
struct DebugCanvas
{
    std::vector<RenderDebugLine> Lines;
    std::vector<RenderDebugBox> Boxes;
    bool ShowWorldGrid = false;
    int WorldGridSlices = 100;
    float WorldGridSpacing = 1.0f;

    void Clear()
    {
        Lines.clear();
        Boxes.clear();
        ShowWorldGrid = false;
    }

    void SubmitLine(RenderDebugLine line) { Lines.push_back(std::move(line)); }
    void SubmitBox(RenderDebugBox box) { Boxes.push_back(std::move(box)); }
};

// Aggregate owned by RendererSubsystem
struct FrameCanvases
{
    SceneCanvas Scene;
    UICanvas UI;
    DebugCanvas Debug;

    void Reset()
    {
        Scene.Clear();
        UI.Clear();
        Debug.Clear();
    }
};
```

### GameplayState Lifecycle
```cpp
// Source: game_framework.md canonical pattern
class GameplayState : public IApplicationState
{
public:
    [[nodiscard]] auto OnEnter(EngineContext& ctx) -> Result<void> override
    {
        // Create Simulation as state subsystem
        // (Registered by plugin, created by SubsystemManifest)
        auto& sim = ctx.GetStateSubsystem<Simulation>();
        m_simulation = &sim;

        // SceneRenderExtractor for ECS -> SceneCanvas bridge
        m_extractor = std::make_unique<SceneRenderExtractor>(sim.GetWorld());

        return {};
    }

    void OnUpdate(EngineContext& ctx, float deltaTime) override
    {
        m_simulation->Update(deltaTime);
    }

    void OnRender(EngineContext& ctx) override
    {
        auto& canvases = ctx.GetAppSubsystem<RendererSubsystem>().GetCanvases();
        m_extractor->Extract(canvases.Scene);
    }

    [[nodiscard]] auto OnExit(EngineContext& ctx) -> Result<void> override
    {
        m_extractor.reset();
        m_simulation = nullptr;
        return {};
    }

    [[nodiscard]] auto GetName() const -> std::string_view override { return "GameplayState"; }

private:
    Simulation* m_simulation = nullptr;
    std::unique_ptr<SceneRenderExtractor> m_extractor;
};
```

### PerformanceOverlay ImGui Pattern
```cpp
class PerformanceOverlay : public IOverlay
{
public:
    [[nodiscard]] auto OnAttach(EngineContext& /*context*/) -> Result<void> override { return {}; }
    [[nodiscard]] auto OnDetach(EngineContext& /*context*/) -> Result<void> override { return {}; }

    void OnUpdate(EngineContext& ctx, float deltaTime) override
    {
        m_accumSeconds += deltaTime;
        ++m_frameCount;

        if (m_accumSeconds >= REFRESH_INTERVAL)
        {
            m_displayFps = static_cast<float>(m_frameCount) / m_accumSeconds;
            m_displayMs = (m_accumSeconds / static_cast<float>(m_frameCount)) * 1000.0f;
            m_accumSeconds = 0.0f;
            m_frameCount = 0;
        }
    }

    void OnRender(EngineContext& /*ctx*/) override
    {
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(180.0f, 60.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Performance", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("%.0f fps", m_displayFps);
        ImGui::Text("%.2f ms", m_displayMs);
        ImGui::End();
    }

    [[nodiscard]] auto GetName() const -> std::string_view override { return "PerformanceOverlay"; }

private:
    static constexpr float REFRESH_INTERVAL = 0.5f;
    float m_accumSeconds = 0.0f;
    uint32_t m_frameCount = 0;
    float m_displayFps = 0.0f;
    float m_displayMs = 0.0f;
};
```

## C++23 Idioms for This Phase

### Deducing `this` for State Accessors
```cpp
// Ref-qualified access to canvases (const and non-const from one definition)
struct FrameCanvases
{
    auto GetScene(this auto&& self) -> decltype(auto) { return std::forward_like<decltype(self)>(self.Scene); }
    auto GetUI(this auto&& self) -> decltype(auto) { return std::forward_like<decltype(self)>(self.UI); }
    auto GetDebug(this auto&& self) -> decltype(auto) { return std::forward_like<decltype(self)>(self.Debug); }
};
```

**Recommendation:** Use deducing `this` conservatively. For FrameCanvases, direct public member access is cleaner than getters (the struct is a simple aggregate). Reserve deducing `this` for cases where const/non-const overload elimination genuinely reduces code (e.g., if canvases had complex access logic). For Phase 5, prefer simple direct member access on aggregates.

### Concepts for Canvas Traits
```cpp
template<typename T>
concept Canvas = requires(T canvas) {
    canvas.Clear();
};

// Validates at registration time that all canvas types satisfy the concept
static_assert(Canvas<SceneCanvas>);
static_assert(Canvas<UICanvas>);
static_assert(Canvas<DebugCanvas>);
```

### Result<void> for Subsystem Init
```cpp
// Already established pattern from Phase 2
[[nodiscard]] auto Initialise(EngineContext& context) -> Result<void> override
{
    m_device = RenderDevice::Create(RenderBackend::Vulkan);
    if (not m_device)
    {
        return MakeError("Failed to create RenderDevice");
    }

    auto& window = context.GetAppSubsystem<WindowSubsystem>();
    if (auto result = m_device->Initialise(window.GetWindow()); not result)
    {
        return std::unexpected(result.error());
    }

    return {};
}
```

### std::generator for Extraction Iteration (Future Consideration)
```cpp
// Could be used for lazy extraction of visible entities
[[nodiscard]] auto ExtractVisibleMeshes(const flecs::world& world,
                                         const Frustum& frustum) -> std::generator<RenderMeshSubmission>
{
    world.each<const Transform, const MeshComponent>([&](const Transform& xform, const MeshComponent& mesh) {
        if (IsInFrustum(frustum, xform.WorldBounds))
        {
            co_yield RenderMeshSubmission{ /* ... */ };
        }
    });
}
```

**Recommendation:** Don't use `std::generator` for the initial extraction path. The current pattern of iterating and pushing into a vector is simpler and sufficient. Generators add overhead per-yield. If extraction becomes a bottleneck, the fix is parallel archetype iteration, not lazy sequences.

### Structured Bindings for Canvas Processing
```cpp
// Renderer processes canvases using structured bindings
void Renderer::ProcessCanvases(const FrameCanvases& canvases)
{
    const auto& [views, meshes, lights, debugDraw, postProcess] = canvases.Scene;
    // Process scene canvas data...
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Monolithic runtime (all services in one class) | Independent subsystems with dependency graph | Widespread by 2020+ (Bevy, O3DE) | Clean headless mode, testability, modularity |
| Flat render submission (one array of draw calls) | Typed/phased submission (scene, UI, debug) | Post-2018 (Bevy, Unity SRP, Wicked) | Render pipeline composability |
| Direct ECS->Renderer coupling | Extraction/proxy pattern | Bevy 0.5+ (2021), UE since inception | Decoupled threading, clean abstraction boundary |
| Global ImGui context management | ImGui through render pipeline passes | Standard practice | Proper render ordering, multi-viewport support |

## Open Questions

1. **ImGui Backend Implementation**
   - What we know: ImGui is in thirdparty/ (via CPM). The engine uses SDL3 and a custom render device (Vulkan-based).
   - What's unclear: Whether an ImGui backend (SDL3 + Vulkan) already exists or needs to be created. ImGui backends typically need platform integration (`ImGui_ImplSDL3_*`) and renderer integration (`ImGui_ImplVulkan_*`).
   - Recommendation: Check if ImGui backends are included in the thirdparty/imgui CPM package. If not, creating a minimal ImGui render backend using the existing RenderDevice is required - this becomes a task in the UIRenderFeature implementation.

2. **How Existing Render Features Adapt to Canvas Input**
   - What we know: Current features read from `FrameRenderParams` which contains `const RenderFrame&`. SceneCanvas replaces/evolves RenderFrame as the input.
   - What's unclear: Whether to modify `FrameRenderParams` to carry canvas data, or create a new params type, or have features read canvases directly.
   - Recommendation: Evolve `FrameRenderParams` to accept canvas references rather than `RenderFrame&`. This is the least disruptive path. Alternatively, since canvases use the same vocabulary types as RenderFrame, the renderer can build a RenderFrame from canvas data as a bridge (allowing Phase 5 to work without touching every render feature).

3. **Application::Loop() Integration Timing**
   - What we know: Phase 6 is the "v2 main loop" phase. Phase 5 creates the subsystems and canvas types.
   - What's unclear: Whether Phase 5 includes a minimal integration path where the new subsystems are exercised (e.g., via tests only) or whether Application::Loop() gets a partial v2 path.
   - Recommendation: Phase 5 creates and tests all new types independently. Application::Loop() changes are Phase 6. This keeps Phase 5 focused on type creation and Phase 6 on integration.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | doctest (already integrated via CMake) |
| Config file | CMakeLists.txt in tests/ |
| Quick run command | `cmake --build --preset debug --target wayfinder_core_tests && ctest --preset test -R core` |
| Full suite command | `cmake --build --preset debug && ctest --preset test` |

### Phase Requirements -> Test Map
| Req ID | Behaviour | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| STATE-06 | GameplayState wraps Simulation lifecycle | unit | `wayfinder_core_tests -tc="GameplayState*"` | No - Wave 0 |
| STATE-07 | EditorState enters and exits cleanly | unit | `wayfinder_core_tests -tc="EditorState*"` | No - Wave 0 |
| OVER-05 | PerformanceOverlay renders via OverlayStack | unit | `wayfinder_core_tests -tc="PerformanceOverlay*"` | No - Wave 0 |
| REND-01 | Five AppSubsystems init in dependency order | unit | `wayfinder_core_tests -tc="EngineSubsystem*"` | No - Wave 0 |
| REND-02 | Canvas types accept submissions and reset | unit | `wayfinder_render_tests -tc="Canvas*"` | No - Wave 0 |
| REND-03 | Render features SetEnabled via capabilities | unit | `wayfinder_render_tests -tc="RenderFeature*SetEnabled*"` | Partial (RenderFeatureTests.cpp exists) |

### Sampling Rate
- **Per task commit:** `cmake --build --preset debug --target wayfinder_core_tests && ctest --preset test -R core`
- **Per wave merge:** `cmake --build --preset debug && ctest --preset test`
- **Phase gate:** Full suite green before `/gsd-verify-work`

### Wave 0 Gaps
- [ ] `tests/app/GameplayStateTests.cpp` - covers STATE-06 (GameplayState lifecycle, Simulation wrapping)
- [ ] `tests/app/EditorStateTests.cpp` - covers STATE-07 (EditorState enter/exit)
- [ ] `tests/app/PerformanceOverlayTests.cpp` - covers OVER-05 (averaging logic, overlay lifecycle)
- [ ] `tests/app/EngineSubsystemTests.cpp` - covers REND-01 (five subsystems, dependency ordering, capability gating)
- [ ] `tests/rendering/CanvasTests.cpp` - covers REND-02 (SceneCanvas, UICanvas, DebugCanvas submission and reset)
- [ ] `tests/rendering/CanvasRenderFeatureTests.cpp` - covers REND-03 (feature SetEnabled via capability transitions)

## Sources

### Primary (HIGH confidence)
- Wayfinder codebase: `EngineRuntime.h/.cpp`, `Simulation.h/.cpp`, `IApplicationState.h`, `IOverlay.h`, `OverlayStack.h`, `SubsystemRegistry.h`, `SubsystemManifest.h`, `RenderFrame.h`, `RenderFeature.h`, `SceneRenderExtractor.h/.cpp`, `FpsOverlayLayer.h/.cpp`, `Renderer.h/.cpp`, `RenderOrchestrator.h`
- Architecture documents: `application_architecture_v2.md`, `game_framework.md`, `application_migration_v2.md`, `rendering_performance.md`
- Phase context: `05-CONTEXT.md` with 21 locked decisions

### Secondary (MEDIUM confidence)
- Bevy render architecture: `docs.rs/bevy/latest/bevy/render` - Extract/Render world pattern
- RavEngine: `include/RavEngine/World.hpp`, `include/RavEngine/App.hpp` - RenderData pattern, App subsystem ownership
- Wicked Engine: `WickedEngine/wiScene.h` - Scene-as-render-data pattern, component manager arrays

### Tertiary (LOW confidence)
- O3DE SystemComponent model (description from documentation, not source verified)
- Unreal 5 subsystem model (general architecture knowledge)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All types build on existing engine primitives (SubsystemRegistry, RenderFrame vocab, IApplicationState, IOverlay)
- Architecture: HIGH - 21 locked decisions from CONTEXT.md leave minimal ambiguity. Existing infrastructure (Phases 1-4) provides the foundation.
- Pitfalls: HIGH - Identified from codebase analysis and engine reference comparison
- C++23 idioms: MEDIUM - Verified against copilot-instructions.md, but practical codegen quality depends on the specific Clang version

**Research date:** 2026-04-05
**Valid until:** 2026-05-05 (stable - all patterns build on existing engine infrastructure)
