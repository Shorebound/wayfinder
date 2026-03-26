---
name: Post-refactor hardening (expanded)
overview: "Production-forward and future-proof rendering pipeline work: CPU-side rename to kill ambiguous \"pass\" terminology, explicit RenderPass capabilities for scheduling/validation, ordered engine pass injection (plugins/slots), plus graph safety, observability, docs, and test hygiene. No revamp of the resource-driven graph model."
todos:
  - id: frame-layer-rename
    content: Rename FramePass → FrameLayerRecord; RenderPassId/RenderPassIds → FrameLayerId/FrameLayerIds (not RenderLayerId — conflicts with scene sort); RenderFrame::Passes → Layers; AddScenePass/AddDebugPass/FindPass → AddSceneLayer/AddDebugLayer/FindLayer; update docs, tests
    status: completed
  - id: render-pass-capabilities
    content: Add RenderPassCapabilities (flags or enum set) + virtual GetCapabilities() on RenderPass; implement for SceneOpaquePass, DebugPass, composition-related paths; document contract for future scheduler/validation
    status: completed
  - id: engine-pass-injection
    content: Replace flat engine pass list with ordered registration (phase/slot + optional order within phase); Renderer API to register engine passes after Initialise; document plugin hook; migrate SceneOpaque/Debug to slots
    status: completed
  - id: graph-invalid-handle
    content: Add dev-time logging/assert or FindHandleChecked for invalid handles during graph setup; document behaviour in render_passes.md
    status: completed
  - id: docs-glossary-params
    content: Expand render_passes.md with new names (FrameLayerRecord vs RenderPass), capabilities, engine injection; RenderPipelineFrameParams contract; cross-link workspace_guide
    status: completed
  - id: dev-execution-order
    content: Log compiled pass order (dev/throttled) after RenderGraph::Compile
    status: completed
  - id: scene-opaque-split
    content: Extract RegisterForwardOpaquePrograms from SceneOpaquePass::OnAttach when touching that file; call from RenderPipeline::Initialise
    status: completed
  - id: tests-nolint
    content: Narrow NOLINT blocks in RenderGraphTests.cpp and RenderPassTests.cpp
    status: completed
  - id: error-policy
    content: Document dev vs shipping behaviour for missing pipelines/textures (fail loud vs fallback)
    status: completed
  - id: resource-typing-followup
    content: Typed graph handles or graph schema version (stronger than FindHandle strings) — can follow capabilities/injection if timeboxed
    status: completed
isProject: true
---

# Post-refactor roadmap (production-forward, expanded scope)

## Guiding principle

Invest where **mistakes become expensive** (wrong graph order, ambiguous types, untestable engine ordering) and avoid **speculative** complexity (full multi-threaded graph build before you have async compile). This plan adds **rename**, **capabilities**, and **engine pass injection** now because they **shape APIs** that are painful to change later; other items stay proportional.

## Context (unchanged architecture)

Keep: **resource-driven `RenderGraph`**, **`RenderPass` injectors** (engine + game), **shared `SubmissionDrawing`**, **composition** in [`RenderPipeline::BuildGraph`](d:\wanderlight\engine\wayfinder\engine\wayfinder\src\rendering\pipeline\RenderPipeline.cpp). The work below **refines** naming and registration — it does not replace the graph.

---

## 1. CPU-side rename (high churn, do now)

**Problem:** Two different meanings of “pass” ([`FramePass`](d:\wanderlight\engine\wayfinder\engine\wayfinder\src\rendering\graph\RenderFrame.h) vs [`RenderPass`](d:\wanderlight\engine\wayfinder\engine\wayfinder\src\rendering\graph\RenderPass.h)) will keep costing reviews and onboarding.

**Recommended names (adjust in implementation if one reads better in call sites):**

| Current | Proposed |
|--------|----------|
| `FramePass` | `FrameLayerRecord` — one logical layer of submissions (scene meshes and/or debug draws) for a view |
| `RenderFrame::Passes` | `RenderFrame::Layers` |
| `AddScenePass` / `AddDebugPass` | `AddSceneLayer` / `AddDebugLayer` |
| `FindPass` / `FindScenePassForSubmission` | `FindLayer` / `FindSceneLayerForSubmission` |

**IDs:** Rename [`RenderPassId`](d:\wanderlight\engine\wayfinder\engine\wayfinder\src\rendering\graph\RenderIntent.h) / `RenderPassIds` in the **same milestone** (per team choice). **Do not** reuse the name `RenderLayerId` for this — it already means **scene sorting layer** (main/overlay) in [`RenderIntent.h`](d:\wanderlight\engine\wayfinder\engine\wayfinder\src\rendering\graph\RenderIntent.h). Use instead:

| Current | Proposed |
|--------|----------|
| `RenderPassId` | `FrameLayerId` |
| `namespace RenderPassIds` | `namespace FrameLayerIds` |

Constants stay semantically the same (`MainScene`, `OverlayScene`, `Debug`). `FrameLayerRecord::Id` type becomes `FrameLayerId`.

**Touch points (mechanical):** [`RenderFrame.h`](d:\wanderlight\engine\wayfinder\engine\wayfinder\src\rendering\graph\RenderFrame.h), [`RenderPipeline.cpp`](d:\wanderlight\engine\wayfinder\engine\wayfinder\src\rendering\pipeline\RenderPipeline.cpp) (Prepare warnings), [`SceneRenderExtractor.cpp`](d:\wanderlight\engine\wayfinder\engine\wayfinder\src\rendering\pipeline\SceneRenderExtractor.cpp), [`RenderResources.cpp`](d:\wanderlight\engine\wayfinder\engine\wayfinder\src\rendering\resources\RenderResources.cpp), tests under `tests/rendering/`, `tests/scene/`.

**Docs:** [`docs/render_passes.md`](d:\wanderlight\engine\wayfinder\docs\render_passes.md) — glossary table at top: **Frame layer record (CPU)** vs **RenderPass (graph injector)** vs **graph node** (`AddPass`).

---

## 2. Capabilities on `RenderPass` (do now)

**Goal:** Avoid `RenderPass` becoming a junk drawer: **declare what each injector can do** so future scheduling, validation, and tooling (profiling buckets, async compute) have a **stable hook** without a second class hierarchy.

**Minimal v1 (flags, extensible):**

- Define `enum class RenderPassCapability : uint32_t` (or `using RenderPassCapabilities = Flags<...>`) with a **small** set, e.g.:
  - `Raster` — emits raster graph nodes
  - `RasterSceneGeometry` — draws scene submissions (opaque/forward path)
  - `RasterOverlayOrDebug` — debug/overlay style
  - `FullscreenComposite` — e.g. composition (if modelled as a pass later)
  - `Compute` — reserved for future compute injectors
- Add `virtual RenderPassCapabilities GetCapabilities() const` on [`RenderPass`](d:\wanderlight\engine\wayfinder\engine\wayfinder\src\rendering\graph\RenderPass.h) with a **sensible default** for game passes (e.g. `Raster` only) or require override — pick one to avoid silent “empty capabilities”.

**Usage now:** Documentation + **dev assert** in optional validators (e.g. “compute pass must not only declare RasterSceneGeometry”) — full enforcement can wait until compute exists.

**Production-forward:** Capabilities are **advisory** at first; later they become **contract** when the executor cares (parallel phases, queue selection).

---

## 3. Engine pass injection / plugins (do now)

**Today:** [`RenderPipeline::Initialise`](d:\wanderlight\engine\wayfinder\engine\wayfinder\src\rendering\pipeline\RenderPipeline.cpp) calls [`AddEnginePass`](d:\wanderlight\engine\wayfinder\engine\wayfinder\src\rendering\pipeline\RenderPipeline.h) twice; order is **implicit list order**. [`Renderer`](d:\wanderlight\engine\wayfinder\engine\wayfinder\src\rendering\pipeline\Renderer.h) only exposes **`AddPass`** for **game** passes.

**Goal:** **Explicit ordering** for engine-owned injectors so **editor plugins / engine modules** can register passes **between** fixed stages without forking `RenderPipeline::Initialise`.

**Recommended model (combine clarity + flexibility):**

- **`enum class EngineRenderPhase`** (or `EnginePassSlot`) — fixed phases with **documented** meaning, e.g. `PreOpaque`, `OpaqueMain`, `PostOpaque`, `Debug`, `PreComposite` (trim to what you need; empty phases need not register).
- **`RegisterEnginePass(EngineRenderPhase phase, int32_t orderWithinPhase, std::unique_ptr<RenderPass> pass)`** — sort key = `(phase, orderWithinPhase, stable tie-break)`.
- **Built-ins:** `SceneOpaquePass` → `OpaqueMain`, `DebugPass` → `Debug` with fixed `orderWithinPhase` so behaviour matches today.
- **Lifecycle:** Same as today: `OnAttach` when registered (after `RenderContext` exists), `OnDetach` on shutdown; **document** whether late registration after first frame is allowed (likely **no** for v1 — register during `Renderer::Initialise` or plugin init before first `Render`).

**Renderer API:** Add **`RegisterEnginePass(...)`** forwarding to `RenderPipeline` so callers do not need a `RenderPipeline*` (currently not exposed on `Renderer`). Keep **`AddPass`** as **game** passes only; name distinction in docs.

**Optional:** `UnregisterEnginePass` by name — only if plugins need teardown; otherwise defer.

---

## 4. Graph invalid handles (still critical)

Unchanged from prior plan: [`ReadTexture`](d:\wanderlight\engine\wayfinder\engine\wayfinder\src\rendering\graph\RenderGraph.cpp) / writes **silently ignore** invalid handles — add **dev ERROR + assert** or `FindHandleChecked`.

---

## 5. Observability, params docs, error policy, tests

- **Compiled order log** after `Compile()` (dev / throttled).
- **`RenderPipelineFrameParams`:** document guarantees in `render_passes.md`.
- **Error policy:** dev loud, shipping structured log + fallback where safe — short section in docs.
- **Narrow NOLINT** in render tests.

---

## 6. Typed graph handles / schema version

Keep as **next** follow-up after the above (still high value, but ordering API + rename are already large). Strong typing for `WellKnown` resources pairs well with **`FindHandleChecked`**.

---

## 7. Explicitly deferred (still correct to defer)

- **Multi-view / view graph** — data still in `RenderFrame`; no fake “view graph” until product needs it.
- **Parallel `AddPasses`** — document “keep setup cheap”; parallelize when profiler says so.

---

## Suggested implementation order

```mermaid
flowchart TD
  rename[FrameLayerRecord rename across codebase]
  caps[RenderPassCapabilities + overrides]
  engine[EngineRenderPhase + RegisterEnginePass on Renderer]
  graph[Invalid handle checks + FindHandleChecked]
  docs[Docs sync: glossary + injection + capabilities]
  obs[Dev execution order log]
  rename --> docs
  caps --> docs
  engine --> docs
  graph --> obs
```

1. **Rename** (mechanical, unblocks doc terminology).
2. **Capabilities** (small surface; tests per pass type).
3. **Engine injection** (refactor `m_enginePasses` to sorted storage; wire `Renderer::RegisterEnginePass`).
4. **Graph validation** + **execution order logging**.
5. **Shader registration split** when editing `SceneOpaquePass` anyway.
6. **Test / tidy** cleanup and **error policy** doc.

---

## Additional note

Renaming **`RenderPass`** (graph injector) was **not** requested — only CPU `FramePass`. If collision with `RenderPass` class name still confuses readers, the glossary is the fix; renaming the **injector** type would be a second large rename (`IRenderGraphPass` / `RenderGraphPassInjector`) — only consider if confusion persists after `FrameLayerRecord`.

**Terminology:** After renames, three concepts are distinct: **`RenderLayerId`** (scene sort: main/overlay), **`FrameLayerId`** (which CPU layer record: main_scene, debug, …), **`RenderPass`** (graph injector class).
