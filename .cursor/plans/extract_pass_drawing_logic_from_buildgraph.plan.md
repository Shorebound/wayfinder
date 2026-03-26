# Plan: Extract Pass Drawing Logic from BuildGraph (#101) — v2

## Summary

Unify engine passes and game passes under a single `RenderPass` type (evolved from the
existing `RenderFeature`). Extract the per-submission draw loop into a composable
`DrawSubmission()` free function. Decompose `BuildGraph` from a 392-line monolith into a
~40-line graph-declaration loop that delegates to self-contained pass objects.

Pure refactor — no visual or functional changes.

## Architecture

### Three layers

```
┌──────────────────────────────────────────────────────────┐
│  RenderPipeline::BuildGraph()        (~40 lines)         │
│  Iterates m_passes → AddPasses() on each                 │
│  Composition stays inline (12 lines, trivial)            │
└──────────────────────────────────────────────────────────┘
                         │
                         ▼
┌──────────────────────────────────────────────────────────┐
│  RenderPass  (single unified type)                       │
│                                                          │
│  Engine-provided:           Game-provided:               │
│  ├─ SceneOpaquePass         ├─ BloomPass                 │
│  ├─ DebugPass               ├─ OutlinePass               │
│  └─ (future: Shadow,        ├─ FogPass                   │
│      Transparent, Sky)      └─ DitherPass                │
└──────────────────────────────────────────────────────────┘
                         │
                         ▼
┌──────────────────────────────────────────────────────────┐
│  DrawSubmission()  — shared composable primitive         │
│  SubmissionDrawState, UBO push, texture bind, draw       │
└──────────────────────────────────────────────────────────┘
```

### Why one type instead of two

- The render graph already determines execution order via resource dependencies.
  Explicit positional ordering is less robust — a missing `ReadTexture` is a bug
  regardless of whether passes live in one list or two.
- `RenderFeature` is architecturally a full pass injection system (reads/writes
  any target, adds compute passes, accesses frame data). Limiting it to
  post-processing is an artificial API surface constraint, not an architectural one.
- "Engine is a library" means the game is a consumer; it doesn't require engine
  passes to be a different type from game passes.
- One concept to learn, one list to manage, one lifecycle to maintain.

### Engine comparison

| Engine | Pattern |
|---|---|
| Filament | `RenderPass` objects with `Executor` — unified graph nodes |
| Bevy | `RenderNode` structs with `run()` — unified graph nodes |
| Unreal | `FMeshPassProcessor` subclasses (two-tier: hardcoded + plugin) |
| Spartan | `Renderer_*` methods per pass on Renderer class |
| Godot | `_render_*` methods on `RendererSceneRenderRD` (two-tier) |

Wayfinder takes the Filament/Bevy approach: one type, graph-ordered.

---

## Detailed Steps

### Phase 1: Evolve RenderFeature → RenderPass

**1a.** Rename `RenderFeature` → `RenderPass` (class, file, all references).
- `RenderFeature.h` → `RenderPass.h` (in `rendering/graph/`)
- `RenderFeatureContext` → `RenderPassContext`
- `RenderFeatureTests.cpp` keeps its test names but updates includes/types
- `Renderer::AddFeature()` → `Renderer::AddPass()`
- `Renderer::RemoveFeature<T>()` → `Renderer::RemovePass<T>()`
- `Renderer::GetFeature<T>()` → `Renderer::GetPass<T>()`
- `m_features` → `m_passes` on Renderer
- `params.Features` → removed from `RenderPipelineFrameParams`
- `render_features.md` → `render_passes.md`

**1b.** Widen `RenderPassContext` to include full `RenderContext&`:
```cpp
struct RenderPassContext
{
    RenderContext& Context;  // full resource infrastructure
};
```
Passes that need specific services (shaders, pipelines, textures, meshes,
transient buffers) access them through `Context`. No God-struct — the context
is the existing service locator the engine already has.

**1c.** Change `GetName()` return from `const std::string&` to `std::string_view`:
```cpp
virtual std::string_view GetName() const = 0;
```
Avoids forcing subclasses to own a `std::string` just for a name. Existing
subclasses can return a string literal directly.

### Phase 2: Move engine passes into RenderPass subclasses

**2a.** Create `SceneOpaquePass` (`rendering/passes/SceneOpaquePass.h/cpp`):
- Implements `RenderPass`.
- `OnAttach`: registers the 3 scene shader programs (unlit, basic_lit,
  textured_lit) currently done in `RenderPipeline::Initialise`.
- `AddPasses`: owns the full `graph.AddPass("MainScene", ...)` — builder setup
  (create transient colour + depth, WriteColour, WriteDepth) AND the execute
  lambda that loops scene submissions and calls `DrawSubmission()`.
- Receives frame data through `AddPasses(graph, frame)` — no extra params needed.
  The execute lambda captures `RenderContext*` (from OnAttach) + frame data.

**2b.** Create `DebugPass` (`rendering/passes/DebugPass.h/cpp`):
- Implements `RenderPass`.
- `OnAttach`: registers the debug_unlit shader; creates the debug line pipeline.
  (Currently this pipeline is created in `Renderer::Initialise` — moves here.)
- `AddPasses`: owns `graph.AddPass("Debug", ...)` — builder setup
  (FindHandle SceneColour/SceneDepth, WriteColour+WriteDepth with LoadOp::Load)
  AND execute lambda (debug lines, grid, boxes).
- Does NOT use `DrawSubmission` — debug geometry has its own simpler draw path.

**2c.** `RenderPipeline::Initialise` creates engine passes and stores them:
```cpp
void RenderPipeline::Initialise(RenderContext& context)
{
    m_context = &context;
    // Engine passes — added in dependency order (graph resolves actual execution)
    AddEnginePass(std::make_unique<SceneOpaquePass>());
    AddEnginePass(std::make_unique<DebugPass>());
}
```

**2d.** Where do engine passes live vs game passes?

`RenderPipeline` owns engine passes (`m_enginePasses`).
`Renderer` owns game passes (`m_passes`, the former `m_features`).
`BuildGraph` iterates engine passes first, then game passes, then composition.
Both are `std::vector<std::unique_ptr<RenderPass>>`.

This is NOT a type distinction — both vectors hold `RenderPass*`. It's an
ownership/lifecycle distinction: engine passes are created once during init
and aren't removable; game passes are added/removed at runtime.

### Phase 3: Extract DrawSubmission()

**3a.** Create `rendering/passes/SubmissionDrawing.h`:
```cpp
struct SubmissionDrawState
{
    RenderDevice& Device;
    PipelineCache& Pipelines;
    ShaderManager& Shaders;
    ShaderProgramRegistry& Programs;
    const RenderPipelineFrameParams& Params;
    const ShaderProgram* LastBoundProgram = nullptr;
    std::vector<uint8_t> MaterialUBOScratch;  // reused across draws
};

/// Draws a single submission (solid + optional wireframe).
/// Resolves shader, binds mesh, pushes UBOs, binds textures, issues draw.
void DrawSubmission(
    SubmissionDrawState& state,
    const RenderMeshSubmission& submission,
    const Matrix4& viewMatrix,
    const Matrix4& projectionMatrix,
    const SceneGlobalsUBO& sceneGlobals);
```

**3b.** Create `rendering/passes/SubmissionDrawing.cpp` — move the ~80-line
per-submission inner loop from current MainScene lambda:
- Shader program resolution
- Fill mode determination
- `pushUniforms` (transform UBO, material UBO merge+serialise, scene globals)
- Mesh resolution (asset vs built-in)
- Pipeline bind + mesh bind
- Texture binding
- Solid draw
- Wireframe variant (MakeWireframeVariant + wireframe draw)

Move into this file's anonymous namespace:
- `TransformUBO`, `UnlitTransformUBO` structs
- `MakeWireframeVariant()` function

**3c.** `SceneOpaquePass::AddPasses` execute lambda becomes:
```cpp
SubmissionDrawState state{device, pipelines, shaders, programs, params};
for (const auto& pass : frame.Passes)
{
    if (!pass.Enabled || pass.Kind != RenderPassKind::Scene) continue;
    // resolve per-pass view/proj from pass.ViewIndex
    for (const auto& submission : pass.Meshes)
    {
        DrawSubmission(state, submission, passView, passProj, sceneGlobals);
    }
}
```
~15 lines. Clean.

### Phase 4: Clean up RenderPipeline

**4a.** `RenderPipeline::BuildGraph` becomes:
```cpp
void RenderPipeline::BuildGraph(RenderGraph& graph,
                                const RenderPipelineFrameParams& params,
                                std::span<const std::unique_ptr<RenderPass>> gamePasses) const
{
    // Engine passes
    for (const auto& pass : m_enginePasses)
    {
        if (pass->IsEnabled())
            pass->AddPasses(graph, params.Frame);
    }

    // Game passes
    for (const auto& pass : gamePasses)
    {
        if (pass->IsEnabled())
            pass->AddPasses(graph, params.Frame);
    }

    // Composition (inline — 12 lines, no shared infrastructure)
    graph.AddPass("Composition", [&](RenderGraphBuilder& builder) { ... });
}
```

**4b.** Remove from `RenderPipeline.cpp`:
- All inline MainScene drawing code (~194 lines)
- All inline Debug drawing code (~123 lines)
- `TransformUBO`, `UnlitTransformUBO`, `DebugMaterialUBO` structs
- `MakeWireframeVariant()` function
- MSVC `#pragma warning` push/pop
- Shader program registration (moved to SceneOpaquePass::OnAttach)

**4c.** `BuildSceneGlobals()` moves to `SceneOpaquePass` (it's only used there).

**4d.** `SceneGlobalsUBO` moves to `SubmissionDrawing.h` (it's a parameter of
`DrawSubmission`, not a pipeline-level concept). If other passes also need
scene globals in the future, it can live in a shared header.

**4e.** Remove `params.Features` from `RenderPipelineFrameParams`. Game passes
are passed as a separate span to `BuildGraph`.

**4f.** Remove `params.DebugLinePipeline` from `RenderPipelineFrameParams`.
The debug line pipeline moves to `DebugPass` (created in its `OnAttach`).

**4g.** Evaluate whether `RenderPipelineFrameParams` still needs to exist or
if its remaining fields can be passed directly.

### Phase 5: Update Renderer integration

**5a.** `Renderer::Render` passes game passes to `BuildGraph`:
```cpp
m_renderPipeline->BuildGraph(graph, params, m_passes);
```

**5b.** `Renderer` no longer owns `m_debugLinePipeline` — moves to `DebugPass`.

**5c.** `Renderer::MakeFeatureContext()` → `Renderer::MakePassContext()`, returns
`RenderPassContext` with full `RenderContext&`.

### Phase 6: File organisation

**New directory**: `engine/wayfinder/src/rendering/passes/`

| File | Contents |
|---|---|
| `rendering/graph/RenderPass.h` | Base class (renamed from RenderFeature.h) |
| `rendering/passes/SceneOpaquePass.h/cpp` | Opaque geometry pass |
| `rendering/passes/DebugPass.h/cpp` | Debug lines + boxes |
| `rendering/passes/SubmissionDrawing.h/cpp` | `DrawSubmission()`, UBO structs, `MakeWireframeVariant()` |

Future passes get their own files in `rendering/passes/`:
- `ShadowDepthPass.h/cpp` — reuses `DrawSubmission` with depth-only program
- `TransparentPass.h/cpp` — reuses `DrawSubmission` with blend + reversed sort
- `SkyPass.h/cpp`, `VelocityPass.h/cpp`, etc.

### Phase 7: Tests

**7a.** Update `RenderFeatureTests.cpp` → `RenderPassTests.cpp`:
- Rename types, verify same lifecycle behaviour
- All existing tests pass with type rename

**7b.** Create `tests/rendering/SubmissionDrawingTests.cpp`:
- DrawSubmission with solid fill mode: verify pipeline bind, mesh bind, UBO push
  (use NullDevice or TrackingRenderDevice from existing tests)
- DrawSubmission with wireframe fill mode: verify wireframe pipeline created + bound
- DrawSubmission with material overrides: verify merged parameters serialised
- DrawSubmission skips submission with invalid mesh: no draw call emitted
- DrawSubmission skips submission with null shader program: no draw call emitted

**7c.** Create `tests/rendering/SceneOpaquePassTests.cpp`:
- AddPasses injects "MainScene" pass into graph
- Execute with empty frame: no crash
- Execute without camera: returns immediately
- Submissions drawn via DrawSubmission (integration)

**7d.** Create `tests/rendering/DebugPassTests.cpp`:
- AddPasses injects "Debug" pass into graph
- Grid vertex generation produces expected count for given slices
- Debug boxes: verify per-box UBO push sequence
- Empty debug draw list: no crash, no draws

**7e.** Verify all existing `RenderPipelineTests.cpp` pass unchanged.

**7f.** Add all new test files to `tests/CMakeLists.txt`.

### Phase 8: Validation

1. `cmake --build --preset debug` — all targets compile
2. `ctest --preset test` — all tests pass
3. `python tools/lint.py` — clean
4. `python tools/tidy.py` — clean
5. Run `journey` sandbox — rendering visually identical
6. Line counts: BuildGraph < 50 lines, each pass's AddPasses < 40 lines,
   DrawSubmission < 100 lines

---

## Files

**Created:**
- `engine/wayfinder/src/rendering/passes/SceneOpaquePass.h`
- `engine/wayfinder/src/rendering/passes/SceneOpaquePass.cpp`
- `engine/wayfinder/src/rendering/passes/DebugPass.h`
- `engine/wayfinder/src/rendering/passes/DebugPass.cpp`
- `engine/wayfinder/src/rendering/passes/SubmissionDrawing.h`
- `engine/wayfinder/src/rendering/passes/SubmissionDrawing.cpp`
- `tests/rendering/SubmissionDrawingTests.cpp`
- `tests/rendering/SceneOpaquePassTests.cpp`
- `tests/rendering/DebugPassTests.cpp`
- `docs/render_passes.md` (evolved from render_features.md)

**Renamed:**
- `rendering/graph/RenderFeature.h` → `rendering/graph/RenderPass.h`
- `tests/rendering/RenderFeatureTests.cpp` → `tests/rendering/RenderPassTests.cpp`

**Modified:**
- `engine/wayfinder/src/rendering/pipeline/RenderPipeline.h` — owns engine passes list, BuildGraph simplified
- `engine/wayfinder/src/rendering/pipeline/RenderPipeline.cpp` — gut inline drawing, delegate to passes
- `engine/wayfinder/src/rendering/pipeline/Renderer.h` — rename feature API → pass API
- `engine/wayfinder/src/rendering/pipeline/Renderer.cpp` — pass context, debug pipeline moves to DebugPass
- `engine/wayfinder/CMakeLists.txt` — add new source files
- `tests/CMakeLists.txt` — add new test files

## Key Decisions

- **Unified type**: One `RenderPass` type for engine + game passes. Ownership
  split (engine list + game list) handles lifecycle differences without a type
  hierarchy. Graph handles ordering.
- **DrawSubmission as free function**: The composable primitive that every geometry
  pass shares. Not a method on a base class — no inheritance needed to draw.
- **SubmissionDrawState over parameter lists**: Focused struct with only what
  DrawSubmission needs (device, pipelines, shaders, programs, params, tracking
  state). Not a God-struct — it's the draw loop's working set.
- **File-per-pass**: Each engine pass is a self-contained translation unit with
  co-located setup + execution. Predictable growth when new passes arrive.
- **Composition stays inline**: 12 lines, no inner loop, no shared infrastructure.
  Becomes CompositionPass when it grows (tone mapping, dithering, film grain).
- **RenderPassContext wraps RenderContext&**: Single indirection to the full
  service infrastructure. No artificial capability restriction on what passes
  can access.
- **DebugMaterialUBO stays with DebugPass**: It's debug-specific, not shared.
  TransformUBO/UnlitTransformUBO move to SubmissionDrawing (shared by all
  geometry passes).
