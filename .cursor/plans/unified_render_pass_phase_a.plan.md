# Plan: Unified Render Pass Pipeline + PostProcessColour Convention (Phase A)

## TL;DR

Replace the early/game/late three-band pass system with a single phase-ordered pass list. Rename `EngineRenderPhase` → `RenderPhase` with 8 bands. Introduce `PostProcessColour` resource convention for chainable post-processing. Remove `PresentSource` / `PresentSourceCopyPass`. Fix `FindHandle` to return latest match for correct resource chaining.

---

## Phase 1: RenderPhase Enum + Unified Pass Storage

**Depends on: nothing. Parallel with nothing — must land first since all other steps reference the new types.**

### Step 1.1 — Rename `EngineRenderPhase` → `RenderPhase` and expand

**File:** `engine/wayfinder/src/rendering/pipeline/RenderPipeline.h`

Replace:
```cpp
enum class EngineRenderPhase : uint8_t {
    PreOpaque = 0,
    OpaqueMain = 1,
    PostOpaque = 2,
    Debug = 3,
    PreComposite = 4,
    LateEngine = 5,
};
```

With:
```cpp
enum class RenderPhase : uint8_t {
    PreOpaque   = 0,   // Shadow maps, GBuffer prep, hi-Z
    Opaque      = 1,   // Main scene geometry (SceneOpaquePass)
    PostOpaque  = 2,   // SSAO, SSR, decals, light clustering
    Transparent = 3,   // Alpha-blended geometry, particles, volumetrics
    PostProcess = 4,   // Bloom, DoF, motion blur, film grain, game effects
    Composite   = 5,   // FXAA/TAA, optional present-source copy
    Overlay     = 6,   // Debug, editor gizmos, UI
    Present     = 7,   // Tonemapping + swapchain blit (exactly one pass)
};
```

Key renames: `OpaqueMain` → `Opaque`, `Debug` → `Overlay`, `PreComposite` removed (subsumed by `PostProcess`/`Composite`), `LateEngine` removed (subsumed by `Composite`/`Present`). Added `Transparent`, `PostProcess`, `Composite`.

### Step 1.2 — Rename `EnginePassSlot` → `PassSlot`, unify storage

**File:** `engine/wayfinder/src/rendering/pipeline/RenderPipeline.h`

Replace `EnginePassSlot` + two vectors with:
```cpp
struct PassSlot {
    RenderPhase Phase = RenderPhase::Opaque;
    int32_t Order = 0;
    uint32_t InsertSequence = 0;
    std::unique_ptr<RenderPass> Pass;
};

std::vector<PassSlot> m_passes;  // Single sorted list
```

Remove: `m_earlyEnginePasses`, `m_lateEnginePasses`, `InvokePassList` private helper, `SortEnginePassList`.

### Step 1.3 — Replace `RegisterEnginePass` with `RegisterPass`

**File:** `engine/wayfinder/src/rendering/pipeline/RenderPipeline.h/.cpp`

New method:
```cpp
void RegisterPass(RenderPhase phase, int32_t order, std::unique_ptr<RenderPass> pass);
```

Implementation: same as current `RegisterEnginePass` minus the early/late split. Appends to `m_passes`, sorts by `(Phase, Order, InsertSequence)`.

Remove: `RegisterEnginePass`.

### Step 1.4 — Simplify `BuildGraph` signature

**File:** `engine/wayfinder/src/rendering/pipeline/RenderPipeline.h/.cpp`

Change from:
```cpp
void BuildGraph(RenderGraph& graph, const RenderPipelineFrameParams& params,
                std::span<const std::unique_ptr<RenderPass>> gamePasses) const;
```

To:
```cpp
void BuildGraph(RenderGraph& graph, const RenderPipelineFrameParams& params) const;
```

Implementation: single loop over `m_passes`, calling `AddPasses` on each enabled pass. No `InvokePassList`, no `gamePasses` parameter.

### Step 1.5 — Update `RenderPipeline::Initialise`

**File:** `engine/wayfinder/src/rendering/pipeline/RenderPipeline.cpp`

```cpp
RegisterPass(RenderPhase::Opaque,  0, std::make_unique<SceneOpaquePass>());
RegisterPass(RenderPhase::Overlay, 0, std::make_unique<DebugPass>());
RegisterPass(RenderPhase::Present, 0, std::make_unique<CompositionPass>());
```

### Step 1.6 — Update `Shutdown` to iterate single list

**File:** `engine/wayfinder/src/rendering/pipeline/RenderPipeline.cpp`

Single loop over `m_passes` calling `OnDetach`, then `m_passes.clear()`.

---

## Phase 2: Renderer API Changes

**Depends on: Phase 1.**

### Step 2.1 — Replace `Renderer::AddPass` and `Renderer::RegisterEnginePass`

**File:** `engine/wayfinder/src/rendering/pipeline/Renderer.h/.cpp`

Remove:
- `void AddPass(std::unique_ptr<RenderPass> pass)` — the "game pass" method
- `void RegisterEnginePass(EngineRenderPhase, int32_t, std::unique_ptr<RenderPass>)` — the engine passthrough
- `std::vector<std::unique_ptr<RenderPass>> m_passes` member

Add:
```cpp
void AddPass(RenderPhase phase, int32_t order, std::unique_ptr<RenderPass> pass);
void AddPass(RenderPhase phase, std::unique_ptr<RenderPass> pass); // order = 0
```

Both delegate to `m_renderPipeline->RegisterPass(...)`. `OnAttach` is called inside `RegisterPass` already (the pipeline has the context).

### Step 2.2 — Update `GetPass<T>` / `RemovePass<T>` to search the pipeline

**File:** `engine/wayfinder/src/rendering/pipeline/Renderer.h`

These templates currently iterate `m_passes` (game-only). Change to expose the pipeline's `m_passes`. Options:
- Add `RenderPipeline::FindPass<T>()` and `RenderPipeline::RemovePass<T>(ctx)` methods
- Or expose an iterator/span from `RenderPipeline` that `Renderer` forwards to

Recommended: add `RenderPipeline::GetPass<T>()` const and non-const, `RenderPipeline::RemovePass<T>(ctx)` that handles `OnDetach` + erase + re-sort if needed. `Renderer::GetPass<T>()` and `Renderer::RemovePass<T>()` forward to pipeline.

### Step 2.3 — Update `Renderer::Render` — drop `m_passes` from `BuildGraph`

**File:** `engine/wayfinder/src/rendering/pipeline/Renderer.cpp`

Change:
```cpp
m_renderPipeline->BuildGraph(graph, params, m_passes);
```
To:
```cpp
m_renderPipeline->BuildGraph(graph, params);
```

### Step 2.4 — Update `Renderer::Shutdown`

Remove the loop detaching `m_passes` — the pipeline's `Shutdown` now handles all passes.

### Step 2.5 — Update `Renderer::Initialise`

Remove the loop calling `OnAttach` on `m_passes` — pipeline handles that in `RegisterPass`.

### Step 2.6 — Update `RenderPass.h` doc comment

**File:** `engine/wayfinder/src/rendering/graph/RenderPass.h`

Change the usage example from `renderer.AddPass(std::make_unique<MyPass>())` to `renderer.AddPass(RenderPhase::PostProcess, 0, std::make_unique<MyPass>())`.

---

## Phase 3: PostProcessColour Resource Convention

**Depends on: Phase 1, parallel with Phase 2.**

### Step 3.1 — Add `PostProcessColour` to `GraphTextureId` and `GraphTextures`

**File:** `engine/wayfinder/src/rendering/graph/RenderGraph.h`

Add to enum:
```cpp
enum class GraphTextureId : uint8_t {
    SceneColour,
    SceneDepth,
    PostProcessColour,  // output of latest post-process pass
};
```

Add to namespace:
```cpp
namespace GraphTextures {
    inline const InternedString SceneColour = InternedString::Intern("SceneColour");
    inline const InternedString SceneDepth  = InternedString::Intern("SceneDepth");
    inline const InternedString PostProcessColour = InternedString::Intern("PostProcessColour");
}
```

Update `GraphTextureIntern` switch to include `PostProcessColour`.

Remove: `PresentSource` enum value, `GraphTextures::PresentSource`, corresponding switch case.

### Step 3.2 — Add `ResolvePostProcessInput` helper

**File:** New header `engine/wayfinder/src/rendering/graph/PostProcessUtils.h`

```cpp
/// Returns PostProcessColour if any prior pass wrote it, else SceneColour.
/// Invalid handle only if neither exists (scene pass didn't run).
inline RenderGraphHandle ResolvePostProcessInput(const RenderGraph& graph)
{
    auto handle = graph.FindHandle(GraphTextureId::PostProcessColour);
    if (handle.IsValid()) return handle;
    return graph.FindHandleChecked(GraphTextureId::SceneColour);
}
```

Inline in a header — no .cpp needed. One function, minimal surface.

### Step 3.3 — Remove `PresentSourceCopyPass`

**Files to delete/remove:**
- `engine/wayfinder/src/rendering/passes/PresentSourceCopyPass.h`
- `engine/wayfinder/src/rendering/passes/PresentSourceCopyPass.cpp`
- Remove from CMakeLists.txt source list

The `PresentSourceCopyPass` was optional and never registered by default. Its role (stable handoff) is replaced by the `PostProcessColour` convention: any post-process pass can write `PostProcessColour` and `CompositionPass` picks it up.

If a stable handoff is still needed, a game can register a trivial fullscreen-copy pass at `RenderPhase::Composite` that reads `PostProcessColour`/`SceneColour` and writes `PostProcessColour`. This is explicit per-game, not engine infrastructure.

### Step 3.4 — Update `CompositionPass` to use `ResolvePostProcessInput`

**File:** `engine/wayfinder/src/rendering/passes/CompositionPass.cpp`

Replace:
```cpp
const RenderGraphHandle presentHandle = graph.FindHandle(GraphTextureId::PresentSource);
const bool usePresentSource = presentHandle.IsValid();
const RenderGraphHandle colourHandle = usePresentSource
    ? presentHandle
    : graph.FindHandleChecked(GraphTextureId::SceneColour);
```

With:
```cpp
#include "rendering/graph/PostProcessUtils.h"
const RenderGraphHandle colourHandle = ResolvePostProcessInput(graph);
```

Remove the `PresentSource` log message. Update `CompositionPass.h` doc comment to reference `PostProcessColour` instead of `PresentSource`.

---

## Phase 4: Fix `FindHandle` Reverse Scan

**Depends on: nothing. Should land in Phase 3 or before, since PostProcessColour chaining relies on it.**

### Step 4.1 — Reverse scan in `FindHandle` (both overloads)

**File:** `engine/wayfinder/src/rendering/graph/RenderGraph.cpp`

Change all `FindHandle` overloads from forward to reverse iteration:

```cpp
RenderGraphHandle RenderGraph::FindHandle(const GraphTextureId id) const
{
    const InternedString& name = GraphTextureIntern(id);
    for (uint32_t i = static_cast<uint32_t>(m_resources.size()); i-- > 0;)
    {
        if (m_resources[i].Name == name)
            return {i};
    }
    return {};
}
```

Same for the `string_view` overload and `FindHandleChecked` overloads.

### Step 4.2 — Reverse scan in `ImportTexture`

**File:** `engine/wayfinder/src/rendering/graph/RenderGraph.cpp`

`ImportTexture(InternedString)` currently does a forward scan and returns first match. Change to reverse scan — returns latest match, or creates if none exist.

**Rationale:** `ImportTexture` is get-or-create. Searching reverse means it finds the latest version of a resource. Creating still appends to the end. This is consistent with `FindHandle`.

---

## Phase 5: CMakeLists.txt + Documentation

**Depends on: Phases 1-4. Parallel within itself.**

### Step 5.1 — Update engine CMakeLists.txt

**File:** `engine/wayfinder/CMakeLists.txt`

- Remove: `passes/PresentSourceCopyPass.h`, `passes/PresentSourceCopyPass.cpp`
- Add: `graph/PostProcessUtils.h`

### Step 5.2 — Update `docs/render_passes.md`

Rewrite sections to reflect:
- `RenderPhase` enum with 8 phases (replace all `EngineRenderPhase` references)
- Unified `AddPass(phase, order, pass)` API (replace `RegisterEnginePass` + `AddPass(pass)` references)
- `PostProcessColour` convention (replace `PresentSource` references)
- `ResolvePostProcessInput` helper
- Remove early/late/game segment language — single ordered list now

### Step 5.3 — Update `RenderPass.h` doc comment

Replace example `renderer.AddPass(std::make_unique<MyPass>())` with `renderer.AddPass(RenderPhase::PostProcess, std::make_unique<MyPass>())`.

---

## Phase 6: Test Updates

**Depends on: all code changes (Phases 1-4). Can be done in parallel per test file.**

### Step 6.1 — Update `tests/rendering/SceneOpaquePassTests.cpp`

- `BuildGraph(graph, params, {})` → `BuildGraph(graph, params)` (remove `gamePasses` argument)
- This test creates a `RenderPipeline`, calls `Initialise`, `Prepare`, then `BuildGraph`. Only the `BuildGraph` signature changes.

### Step 6.2 — Update `tests/rendering/RenderPassTests.cpp`

- The `TrackingRenderDevice` and `TestPass`/`OverlayPass` test helpers don't reference `EngineRenderPhase` directly
- "Renderer wraps the frame in a GPU debug group" test: creates a `Renderer`, calls `Render(frame)` — no signature change needed since `Render` is unchanged
- "RenderPass default state" / "Pass injects passes" / "Multiple passes inject in registration order" — these don't use `Renderer::AddPass`, they manage passes manually. No change needed.
- "Pass executes in compiled graph" — doesn't use pipeline API. No change.

### Step 6.3 — Update `tests/rendering/RenderPipelineTests.cpp`

- Tests reference `RenderPipeline` directly via `Initialise`/`Prepare`/`Shutdown` — these don't call `BuildGraph` or `RegisterEnginePass` in the tests themselves.
- The pipeline tests should still pass since `Initialise`/`Prepare`/`Shutdown` internal changes are transparent.
- "Extractor routes blended materials" test creates a `RenderPipeline` and calls `Prepare` — no `BuildGraph` call, just `Prepare`. Unchanged.

### Step 6.4 — New tests (add to `tests/rendering/RenderPipelineTests.cpp` or new file)

1. **Phase ordering test:** Register 4 passes at different phases (PostProcess:100, Opaque:0, Present:0, PostProcess:0). Call `BuildGraph`. Verify `AddPasses` call order is (Opaque:0, PostProcess:0, PostProcess:100, Present:0).

2. **Same-phase ordering tiebreak:** Register 2 passes at PostProcess:0 in sequence. Verify first-registered calls `AddPasses` before second-registered (InsertSequence tiebreak).

### Step 6.5 — New tests for FindHandle reverse scan (add to `tests/rendering/RenderGraphTests.cpp`)

1. **FindHandle returns latest:** Two `CreateTransient` calls with same name → `FindHandle` returns the handle with the higher index.

2. **ImportTexture returns latest:** After creating two resources with same name, `ImportTexture` returns the later one (no new resource created).

### Step 6.6 — New tests for PostProcessColour (add to `tests/rendering/RenderGraphTests.cpp`)

1. **ResolvePostProcessInput returns SceneColour when no PostProcessColour:** Create SceneColour resource, call `ResolvePostProcessInput` → returns SceneColour handle.

2. **ResolvePostProcessInput returns PostProcessColour when present:** Create SceneColour and PostProcessColour resources → returns PostProcessColour handle.

3. **PostProcessColour chaining:** Two passes both read via `ResolvePostProcessInput` and write PostProcessColour. Graph compiles, topo order correct (second reads first's output).

---

## Relevant Files

### Modified
- [engine/wayfinder/src/rendering/pipeline/RenderPipeline.h](engine/wayfinder/src/rendering/pipeline/RenderPipeline.h) — `RenderPhase` enum, `PassSlot`, unified `m_passes`, `RegisterPass`, `BuildGraph` signature
- [engine/wayfinder/src/rendering/pipeline/RenderPipeline.cpp](engine/wayfinder/src/rendering/pipeline/RenderPipeline.cpp) — `RegisterPass`, `BuildGraph` single-loop, `Initialise` updated registrations, `Shutdown` single-loop
- [engine/wayfinder/src/rendering/pipeline/Renderer.h](engine/wayfinder/src/rendering/pipeline/Renderer.h) — `AddPass(phase, order, pass)` + `AddPass(phase, pass)`, remove old `AddPass(pass)`, remove `RegisterEnginePass`, remove `m_passes`, update `GetPass`/`RemovePass` to delegate to pipeline
- [engine/wayfinder/src/rendering/pipeline/Renderer.cpp](engine/wayfinder/src/rendering/pipeline/Renderer.cpp) — `AddPass` delegates to pipeline, `Render` drops `m_passes`, `Shutdown`/`Initialise` game-pass loops removed
- [engine/wayfinder/src/rendering/graph/RenderGraph.h](engine/wayfinder/src/rendering/graph/RenderGraph.h) — `PostProcessColour` in enum + namespace, remove `PresentSource`
- [engine/wayfinder/src/rendering/graph/RenderGraph.cpp](engine/wayfinder/src/rendering/graph/RenderGraph.cpp) — `FindHandle`/`FindHandleChecked`/`ImportTexture` reverse scan
- [engine/wayfinder/src/rendering/passes/CompositionPass.h](engine/wayfinder/src/rendering/passes/CompositionPass.h) — doc comment update
- [engine/wayfinder/src/rendering/passes/CompositionPass.cpp](engine/wayfinder/src/rendering/passes/CompositionPass.cpp) — use `ResolvePostProcessInput`
- [engine/wayfinder/src/rendering/graph/RenderPass.h](engine/wayfinder/src/rendering/graph/RenderPass.h) — doc comment update
- [engine/wayfinder/CMakeLists.txt](engine/wayfinder/CMakeLists.txt) — remove PresentSourceCopyPass files, add PostProcessUtils.h
- [docs/render_passes.md](docs/render_passes.md) — full rewrite of registration/ordering/resource convention sections
- [tests/rendering/SceneOpaquePassTests.cpp](tests/rendering/SceneOpaquePassTests.cpp) — `BuildGraph` signature
- [tests/rendering/RenderPipelineTests.cpp](tests/rendering/RenderPipelineTests.cpp) — add phase ordering tests
- [tests/rendering/RenderGraphTests.cpp](tests/rendering/RenderGraphTests.cpp) — add FindHandle reverse + PostProcessColour tests

### New
- `engine/wayfinder/src/rendering/graph/PostProcessUtils.h` — `ResolvePostProcessInput` inline helper

### Deleted
- `engine/wayfinder/src/rendering/passes/PresentSourceCopyPass.h`
- `engine/wayfinder/src/rendering/passes/PresentSourceCopyPass.cpp`

---

## Verification

1. **Build:** `cmake --build --preset debug` — zero errors
2. **Tests:** `ctest --preset test` — all existing tests pass, new tests pass
3. **Lint:** `python tools/lint.py --changed` — clean
4. **Tidy:** `python tools/tidy.py --changed` — clean
5. **Manual:** Run `journey` sandbox, verify scene renders correctly (SceneOpaquePass → DebugPass → CompositionPass chain compiles and executes)
6. **Grep:** No remaining references to `EngineRenderPhase`, `RegisterEnginePass`, `PresentSource`, `PresentSourceCopyPass`, `m_earlyEnginePasses`, `m_lateEnginePasses`, `gamePasses` in source files

---

## Decisions

- **Single unified pass list.** No engine/game split. All passes register with `(RenderPhase, order)`.
- **`RenderPhase` replaces `EngineRenderPhase`.** 8 phases: PreOpaque → Opaque → PostOpaque → Transparent → PostProcess → Composite → Overlay → Present.
- **`PostProcessColour` replaces `PresentSource`.** Convention: post-process passes read via `ResolvePostProcessInput`, write `PostProcessColour`. CompositionPass reads the result.
- **PresentSourceCopyPass removed.** Was optional and never default-registered. The PostProcessColour convention replaces its role.
- **FindHandle + ImportTexture reverse scan.** Returns latest match — enables same-named resource chaining for PostProcessColour.
- **GetPass/RemovePass search all passes** including engine passes via the pipeline's unified list.
- **Breaking API.** `Renderer::AddPass(phase, order, pass)` replaces both `AddPass(pass)` and `RegisterEnginePass(phase, order, pass)`.
