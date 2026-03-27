---
name: Pluggable composition + post path
overview: Replace ad-hoc pipeline ordering with explicit injection segments, extract CompositionPass, introduce PresentSource and compile-time validation, and wire post-processing toward typed effects and production-grade contracts—no legacy constraints; elegance, modularity, extensibility, and production-readiness are explicit goals.
todos:
  - id: pipeline-segments
    content: Introduce ordered injection segments (early engine / game / late engine) in BuildGraph; avoid N separate loops by iterating segment lists or one flattened ordered queue built at registration time
    status: completed
  - id: late-engine-phase
    content: Add EngineRenderPhase::LateEngine (or PostGameEngine)—semantics "runs after game passes"; document vs PreComposite; register CompositionPass in this band only
    status: pending
  - id: composition-pass
    content: Add CompositionPass.{h,cpp}; move composition shader registration to OnAttach; sample PresentSource with fallback to SceneColour
    status: pending
  - id: graph-texture-present
    content: Add GraphTextureId::PresentSource (+ GraphTextures name); document last-writer contract; optional no-op pass or convention when no post chain
    status: pending
  - id: compile-validation
    content: RenderGraph::Compile—dev assert or error if zero SwapchainWrite passes; optional warn if multiple swapchain writers
    status: pending
  - id: postprocess-typed
    content: Replace string-keyed runtime PostProcessEffect bag with effect type ids / variant of known structs + authoring adapter from JSON/strings
    status: pending
  - id: postprocess-gpu-bridge
    content: CompositionPass (or shared helper) reads primary view PostProcessStack and pushes uniforms/constants for grading/CA/etc. as shaders land
    status: pending
  - id: cmake-docs-tests
    content: CMakeLists, render_passes.md overhaul (segments, PresentSource, validation, typed effects), SceneOpaquePassTests + render tests + lint/tidy
    status: completed
isProject: false
---

# Pluggable composition, pipeline structure, and post path (full scope)

## Goals (explicit)

- **Elegance:** One clear model for “who runs when” (no hidden special cases in `BuildGraph`).
- **Modularity:** Passes own programs/resources; pipeline describes **segments**, not scattered policy.
- **Extensibility:** Game and engine injectors + stable graph resource ids + typed post data at the GPU boundary.
- **Production-readiness:** Validation in `Compile`, explicit present input, no silent empty graphs, contracts documented for multi-view growth.

**Non-goals:** Backward compatibility, migration shims, or preserving old APIs.

---

## Problem: order of injection

Today [`RenderPipeline::BuildGraph`](engine/wayfinder/src/rendering/pipeline/RenderPipeline.cpp) runs **all engine passes** (by phase) → **game passes** → **inline Composition**. Moving composition into the same single engine loop would run it **before** game passes.

**Required ordering:**

```mermaid
flowchart LR
  earlyEngine[Early engine injectors]
  gamePasses[Game injectors]
  lateEngine[Late engine injectors]
  earlyEngine --> gamePasses --> lateEngine
```

---

## 1. Prefer pipeline segments over triple loops

**Avoid** three copy-pasted `for` loops that drift apart over time.

**Preferred:** Represent injection as **ordered segments**, each a list of `RenderPass*` (or slots with metadata). `BuildGraph` does:

```text
for (segment : ordered_segments)
  for (pass : segment)
    if (enabled) pass->AddPasses(graph, params);
```

Segments are fixed at pipeline init: **`EarlyEngine`**, **`Game`**, **`LateEngine`** (names illustrative). Engine registration APIs append into `EarlyEngine` or `LateEngine` according to phase; `Renderer` still owns the **`Game`** segment (`m_passes`).

**Alternative (equivalent):** At `RegisterEnginePass` / `AddPass`, append to a **single ordered queue** with an explicit `InjectionSlot` tag (`EarlyEngine`, `Game`, `LateEngine`). One loop over the queue preserves order without nested loops.

**Phase enum:** Keep [`EngineRenderPhase`](engine/wayfinder/src/rendering/pipeline/RenderPipeline.h) for **sorting within** `EarlyEngine` only (PreOpaque → … → PreComposite). Add **`LateEngine`** (or `PostGameEngine`) for passes that **must** run after game injectors. **`LateEngine` is not “one more band in the same sweep”—it is a different segment.**

Naming recommendation: **`LateEngine`** reads clearer than `FinalComposite` (“composite” is an implementation detail).

---

## 2. `CompositionPass` class

Add [`engine/wayfinder/src/rendering/passes/CompositionPass.h`](engine/wayfinder/src/rendering/passes/CompositionPass.h) / `.cpp`:

- Subclass [`RenderPass`](engine/wayfinder/src/rendering/graph/RenderPass.h).
- **`AddPasses`:** `FindHandle` **`PresentSource`** first; if invalid, **`FindHandleChecked(SceneColour)`** (or documented fallback path). `ReadTexture`, `DeclarePassCapabilities(Raster | FullscreenComposite)`, `SetSwapchainOutput`, fullscreen draw.
- **`OnAttach`:** Register `composition` / `fullscreen` [`ShaderProgramDesc`](engine/wayfinder/src/rendering/pipeline/RenderPipeline.cpp) here so the pass owns its GPU-facing registration.
- Register via **`RegisterEnginePass(EngineRenderPhase::LateEngine, 0, …)`** in [`RenderPipeline::Initialise`](engine/wayfinder/src/rendering/pipeline/RenderPipeline.cpp) (only after segment plumbing exists).

Remove the inline `graph.AddPass("Composition", …)` block entirely.

---

## 3. `GraphTextureId::PresentSource`

In [`RenderGraph.h`](engine/wayfinder/src/rendering/graph/RenderGraph.h) / [`GraphTextures`](engine/wayfinder/src/rendering/graph/RenderGraph.h):

- Add **`PresentSource`** — the **colour buffer composition samples** before swapchain output.
- **Contract:** If no post chain runs, **`SceneColour`** is aliased or copied into **`PresentSource`**, **or** the last writer of the post stack writes **`PresentSource`**. Simplest **first implementation:** add a **trivial pass** in `LateEngine` before `CompositionPass` that, when no prior pass wrote `PresentSource`, **copies** `SceneColour` → `PresentSource` (or **import** the same handle under both names if the graph supports safe aliasing—only if the render graph semantics allow it; otherwise one blit pass).

Document in [`docs/render_passes.md`](docs/render_passes.md): **CompositionPass always reads `PresentSource`**, never `SceneColour` directly, so game/engine post passes always target **`PresentSource`** as their final output (or the copy pass bridges).

---

## 4. `RenderGraph::Compile` validation (required in dev)

- If **no** pass sets **`SwapchainWrite`**: **error** (non-`NDEBUG`) and **structured log** in shipping—do not silently compile a graph that presents nothing useful.
- If **more than one** pass sets swapchain output: **warn** or **error** in dev (choose one policy and document).

This addresses dead-pass culling hiding a missing present pass.

---

## 5. Typed post-processing (runtime boundary)

Today [`PostProcessEffect`](engine/wayfinder/src/rendering/materials/PostProcessVolume.h) uses **`std::string` type** + **`unordered_map` params**—fine for authoring, weak for **shader binding** and **branches**.

**Target:**

- **`PostProcessEffectType`** as **enum class** or **stable `uint32_t` id** registered in one place.
- **Parameters** as **`std::variant`** of **small structs** per effect (BloomParams, ColourGradingParams, …) **or** a **typed map** keyed by param id—not string keys in hot code.
- **Authoring path** (JSON / scene): keep human-readable strings in loaders; **resolve to typed** values when building `RenderView::PostProcess` (or a renamed `ViewPostProcessState`).

This can land in **stages**: (1) ids + variant envelope, (2) migrate JSON loader in [`ComponentRegistry`](engine/wayfinder/src/scene/ComponentRegistry.cpp), (3) update blend code in [`PostProcessVolume.cpp`](engine/wayfinder/src/rendering/materials/PostProcessVolume.cpp).

---

## 6. GPU bridge: `PostProcessStack` → composition

- **`CompositionPass`** (or a shared **`PostProcessUniforms`** helper) reads **`ResolvePreparedPrimaryView(params.Frame)`** and the primary view’s **typed** post state.
- Push **constants / UBO** for grading, CA, etc., as shaders gain interfaces—**no** string lookups in the draw lambda.

---

## 7. Multi-view (contract, not full implementation)

Document **now**: for split-screen, either **one graph per view** or **view-indexed resources**—do not let `PresentSource` imply a single implicit view without a stated rule. Implementation can follow; the plan **reserves** the decision in `render_passes.md`.

---

## 8. Developer experience

- **Docs:** Rewrite [`docs/render_passes.md`](docs/render_passes.md): segments, `LateEngine`, `PresentSource`, validation, typed post, primary-view sampling for composition.
- **Renderer API:** Consider replacing **`GetPass<T>()`** `dynamic_cast` with **type tags** or **explicit handles** when you touch `Renderer`—optional cleanup in same effort if low cost.

---

## 9. Tests and tooling

- [`tests/rendering/SceneOpaquePassTests.cpp`](tests/rendering/SceneOpaquePassTests.cpp): still must **`Compile`** after pipeline changes.
- Add tests: **missing swapchain writer** fails compile in dev; **PresentSource** path resolves when only scene colour exists (copy or fallback).
- **`tools/lint.py`** / **`tools/tidy.py`** after substantive edits.

---

## Architectural alignment (Bevy-like *invariants*, not clone)

- **Extract → graph → present** remains the story; **`RenderFrame`** stays the render-facing aggregate (no requirement for a second ECS world).
- **Coarse `RenderPass`** injectors remain valid; **fine-grained “every draw is a node”** is optional tooling-scale, not a requirement.

---

## Implementation order (suggested)

1. **Segments + `LateEngine` + `CompositionPass` + remove inline composition** (behaviour match + `PresentSource` fallback = `SceneColour` until copy pass exists).
2. **`PresentSource` + copy/blit or last-writer rule** + composition reads `PresentSource` only.
3. **Compile validation** for swapchain.
4. **Typed post** + loader migration.
5. **Uniform bridge** + shaders as needed.

Steps 1–3 unblock a **correct, extensible** frame; 4–5 complete the **post stack** story.
