# Render pipeline and render features

This document explains how frame rendering is structured in Wayfinder, what a **render feature** is, and how to add one—especially post-process work that reads **blendable effect** data from the scene.

For repository layout and build commands, see [Workspace guide](workspace_guide.md).

---

## What you are extending

The renderer builds a **render graph** each frame: a list of GPU passes with explicit texture reads and writes. The graph is **compiled** (dependency order, dead-pass culling) and then **executed**.

**Render features** are the plug-in points. A feature subclasses `RenderFeature` (`engine/wayfinder/src/rendering/graph/RenderFeature.h`), registers with a **phase** and optional **order**, and each frame calls `AddPasses` to append one or more graph passes. Engine code and game/editor code use the same type; the default pipeline is assembled in `RenderOrchestrator::Initialise` (`engine/wayfinder/src/rendering/pipeline/RenderOrchestrator.cpp`).

Rough flow each frame:

1. **Prepare** — `RenderOrchestrator::Prepare` validates views, fills layer records, culls and sorts draws, and supplies `FrameRenderParams::PrimaryView` for graph code.
2. **Build graph** — For every registered feature in phase order, `AddPasses` runs (if the feature is enabled) and records passes and resource dependencies.
3. **Compile & execute** — `RenderGraph::Compile` orders passes by dependencies; `Execute` runs them on the GPU.

**CPU registration order** (phase → order → insert sequence) only orders when **`AddPasses` runs**. **GPU order** follows **resource dependencies** (`ReadTexture`, `WriteColour`, etc.). If two passes do not depend on each other, the compiler may order them arbitrarily—declare every read/write you rely on.

---

## Terminology

| Term | Meaning |
|------|---------|
| **`RenderFeature`** | Injectable unit that adds graph passes via `AddPasses`. |
| **`RenderPhase`** | Fixed band for **registration** ordering (`PreOpaque` … `Present`). See `RenderOrchestrator.h`. |
| **`FrameRenderParams`** | Per-frame bundle passed into `AddPasses` (frame, swapchain size, built-in meshes, resource cache, primary view). `FrameRenderParams.h`. |
| **`RenderFrame` / `RenderView`** | CPU-side frame description; each view has a **`BlendableEffectStack`** at `RenderView::PostProcess` for post-process parameters blended from volumes. |
| **`FrameLayerRecord` / `FrameLayerId`** | Per-view layer of mesh submissions (main scene, overlay, debug, …). Distinct from **`RenderLayerId`** on a mesh (sorting layer). |
| **`GraphTextureId`** | Stable names for main colour/depth and the post chain handoff (`SceneColour`, `SceneDepth`, `PostProcessColour`). |
| **Blendable effect** | A typed, name-registered parameter struct blended from `BlendableEffectVolumeComponent` instances into `BlendableEffectStack`, then read in shaders/features. |

---

## Default pipeline (engine)

After `RenderOrchestrator::Initialise`, the built-in registration order is:

| Phase | Order | Feature |
|-------|------|---------|
| `Opaque` | 0 | `SceneOpaquePass` — writes `SceneColour` / `SceneDepth`. |
| `PostProcess` | 800 | `ChromaticAberrationFeature` |
| `PostProcess` | 900 | `VignetteFeature` |
| `Composite` | 0 | `ColourGradingFeature` |
| `Overlay` | 0 | `DebugPass` |
| `Present` | 0 | `CompositionPass` — samples the latest post-process input and **writes the swapchain** (`SetSwapchainOutput`). |

Game or editor code adds more features with `Renderer::AddPass` (see below). Lower **`order`** in the same phase runs **earlier** among features.

### Phase guide (what to register where)

| `RenderPhase` | Typical content |
|---------------|-----------------|
| `PreOpaque` | Shadow maps, G-buffer prep, hi-Z. |
| `Opaque` | Main scene geometry. |
| `PostOpaque` | SSAO, SSR, decals. |
| `Transparent` | Alpha-blended geometry, particles. |
| `PostProcess` | Fullscreen or spatial effects that chain through **`PostProcessColour`** (bloom, aberration, vignette, …). |
| `Composite` | Colour-space / grading-style work that still feeds the same handoff texture before present. |
| `Overlay` | Debug, gizmos, UI. |
| `Present` | Normally **one** pass that calls **`SetSwapchainOutput`** (the built-in `CompositionPass`). |

---

## Registering and controlling features

### API

Use **`Renderer`** from game or editor code:

- **`AddPass(RenderPhase, int32_t order, std::unique_ptr<RenderFeature>)`** — preferred when you need ordering within a phase.
- **`AddPass(RenderPhase, std::unique_ptr<RenderFeature>)`** — same as `order == 0`.

Passes registered **before** `Renderer::Initialise` are **deferred** and attached when the orchestrator initialises.

### Enable and disable

```cpp
auto* feature = renderer.GetPass<MyFeature>();
if (feature)
{
    feature->SetEnabled(false); // AddPasses is not called; no graph nodes for this feature.
}
```

### Removal

```cpp
renderer.RemovePass<MyFeature>(); // OnDetach runs for attached passes.
```

### Compile rule (swapchain)

A successful graph compile expects **at least one** pass that calls **`SetSwapchainOutput`**. In non-`NDEBUG` builds, more than one swapchain writer typically produces a **warning**. The default `CompositionPass` satisfies this.

---

## `FrameRenderParams` (contract for `AddPasses`)

| Field | Role |
|-------|------|
| `Frame` | `RenderFrame`: views, layers, lights, per-view post stacks. |
| `SwapchainWidth` / `SwapchainHeight` | Extents for transient targets; non-zero when rendering. |
| `BuiltInMeshes` | `BuiltInMeshTable`: engine primitives for passes that draw them. |
| `ResourceCache` | May be null; passes that need asset meshes must tolerate null. |
| `PrimaryView` | From `Rendering::ResolvePreparedPrimaryView` — matrices and related defaults for the primary view. |

---

## Graph resources and the post-process chain

### Engine texture names (`GraphTextureId`)

These use stable interned names (see `RenderGraph.h`):

| Id | Role |
|----|------|
| `SceneColour` | Main HDR-ish scene target from `SceneOpaquePass`. |
| `SceneDepth` | Scene depth. |
| `PostProcessColour` | **Logical name for the latest post-process output.** Each new transient created with this debug name becomes the handle returned by `FindHandle(PostProcessColour)` until another resource replaces it in the graph. |

Use **`graph.FindHandle(GraphTextureId::…)`** or **`FindHandleChecked`** when the resource must exist (checked variant logs and asserts in development when missing).

### Chaining post-process passes

- **`ResolvePostProcessInput(graph)`** — Returns the handle for **`PostProcessColour`** if any prior pass created it this frame; otherwise **`SceneColour`** (via `FindHandleChecked` for scene colour). Use this as the fullscreen **input** after the opaque pass.
- **`CreatePostProcessOutput(builder, width, height)`** — Allocates a new **`PostProcessColour`** transient in **`RGBA16_FLOAT`** (handoff between post steps; reduces banding).

The built-in **`CompositionPass`** only **samples** `ResolvePostProcessInput` and blits to the swapchain (`fullscreen` + `fullscreen_copy` program `composition_blit`). It does **not** read blendable stacks; colour grading, vignette, and chromatic aberration are separate features earlier in the pipeline.

### Capabilities (dev-time checks)

`RenderFeature::GetCapabilities()` describes the feature for documentation and tooling. Per **graph pass**, call **`builder.DeclarePassCapabilities(mask)`** inside the pass setup so `RenderGraph::Compile` can validate behaviour in non-`NDEBUG` builds (e.g. fullscreen composite, scene geometry). Omit if the pass does not need checks.

---

## Blendable effects (scene → GPU)

Volumes and stacks are the data path from level design to your shader parameters.

### Data flow

1. **`BlendableEffectVolumeComponent`** (`volumes/BlendableEffect.h`) holds effect instances referencing registered types by name.
2. During scene extraction, volumes contribute to **`RenderView::PostProcess`** as a **`BlendableEffectStack`** (weighted blend of payloads).
3. A post feature resolves its payload for the view (see below) and pushes uniforms / constants in the execute lambda.

### Registry and sealing

- **`BlendableEffectRegistry`** (`volumes/BlendableEffectRegistry.h`) stores **effect type identity**, **blend (Lerp)**, and **JSON (de)serialisation** for each registered payload type.
- **`Renderer::Render`** calls **`SealBlendableEffects()`** on the **first** frame if not already sealed. You may call **`Renderer::SealBlendableEffects()`** yourself after all types are registered.
- Register **only** from **`RenderFeature::OnAttach`** (or other init that runs before the first render / seal)—not mid-frame.

Access the registry from **`RenderServices`** via **`GetBlendableEffectRegistry()`** on the `RenderFeatureContext`. **`BlendableEffectRegistry::SetActiveInstance` / `GetActiveInstance`** exist for legacy/global consumers (e.g. some component validation); prefer dependency injection where possible.

### Registering a payload type

1. Define a **trivially copyable** payload struct (often with **`Override<T>`** fields for artist overrides). See **`RenderingEffects.h`** for built-in examples (`ChromaticAberrationParams`, `VignetteParams`, `ColourGradingParams`).
2. Provide **`BlendableEffectTraits<T>`** — often via **`FIELDS`** reflection (`OverrideReflection.h`); specialise only when you need legacy JSON keys or non-default behaviour.
3. In **`OnAttach`**: `ctx.Context.GetBlendableEffectRegistry()->Register<MyPayload>("my_effect_name")` and store the returned **`BlendableEffectId`** if you need it for stack lookups.

### Resolving parameters in `AddPasses`

Use **`ResolveEffect<TPayload>(stack, effectId)`** from **`volumes/BlendableEffectUtils.h`**: returns the blended payload or **identity** if the effect is absent.

Built-in post features typically take **`params.Frame.Views.front().PostProcess`** when **`PrimaryView.Valid`** and the view list is non-empty; otherwise they use an empty stack. **Multi-view** post-process is not fully generalised—if you add split-screen, decide per-view stacks and resources explicitly.

### Shaders and programs

Register **`ShaderProgramDesc`** entries in **`OnAttach`** through **`RenderServices::GetPrograms()`**, matching your fragment shader and UBO layout. See **`ChromaticAberrationFeature.cpp`** for a full minimal pattern (register blendable type, register program, read stack in `AddPasses`, early-out on identity / zero intensity, `ResolvePostProcessInput` → `CreatePostProcessOutput`, fullscreen draw in execute lambda).

---

## Implementing a new `RenderFeature`

1. **Subclass** `RenderFeature`: implement **`GetName()`**, **`AddPasses(...)`**, and usually **`OnAttach` / `OnDetach`** for programs and registry.
2. **Register** with `Renderer::AddPass` at the appropriate **`RenderPhase`** and **`order`**.
3. In **`AddPasses`**, for each GPU pass:
   - Declare **reads** and **writes** on `RenderGraphBuilder`.
   - Return a lambda **`void(RenderDevice&, const RenderGraphResources&)`** that binds pipelines, samplers, and draws (or dispatch for compute).
4. If the feature uses blendable data, **register the type in `OnAttach`** and **resolve** from `RenderView::PostProcess` in `AddPasses`.
5. For post chains, use **`ResolvePostProcessInput`** and **`CreatePostProcessOutput`** so later passes and `CompositionPass` see a consistent handoff.

Disabled features skip `AddPasses` entirely—no graph nodes, no cost beyond the feature object.

---

## Testing

- Use the **Null** backend and **`RenderDevice::Create(RenderBackend::Null)`** for headless tests; no window or GPU required.
- **`tests/rendering/RenderFeatureTests.cpp`** — general feature patterns.
- **`tests/rendering/PostProcessFeatureTests.cpp`** — registration of built-in post features, blendable names, and shader program names.

---

## Error handling notes

- **Development:** Prefer **`FindHandleChecked`**, clear logs when optional GPU state is missing, and use **`DeclarePassCapabilities`** to catch mis-wired passes early.
- **Shipping (`WAYFINDER_SHIPPING`):** Prefer structured warnings and skipping draws over crashing; avoid silent failure without a log line when something unexpected is missing.
