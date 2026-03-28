# Render Features

## Glossary (read this first)

| Term | Meaning |
|------|---------|
| **`FrameLayerRecord` / `RenderFrame::Layers`** | CPU-side per-view layer of mesh submissions and/or debug draws (`RenderFrame.h`). **Not** the graph injector. |
| **`RenderLayerId` / `RenderLayers::Main`** | Scene **sorting** layer (e.g. main vs overlay) on a `RenderMeshSubmission`. |
| **`FrameLayerId` / `FrameLayerIds::MainScene`** | Which **logical layer record** (main_scene, overlay_scene, debug, …). |
| **`RenderFeature` (graph injector)** | Subclass of `rendering/graph/RenderFeature.h` — registers `AddPasses` to inject **graph nodes**. Engine and game use the same type. |
| **Graph node** | One `RenderGraph::AddPass` / `AddComputePass` entry for a frame. |
| **`RenderCapabilities` / `DeclarePassCapabilities`** | Bitmask describing injector behaviour. Graph nodes call `RenderGraphBuilder::DeclarePassCapabilities` for dev-time checks in `RenderGraph::Compile`. |
| **`PreparedPrimaryView` / `ResolvePreparedPrimaryView`** | Primary view matrices and clear colour after `Prepare` (`rendering/graph/RenderFrameUtils.h`); bundled on `FrameRenderParams::PrimaryView`. |
| **`RenderPhase`** | Ordering band for `RenderOrchestrator::RegisterPass` / `Renderer::AddPass` — a single sorted list `(phase, order, registration sequence)`. |

See also: [Workspace guide](workspace_guide.md) for repo layout.

## Purpose

A `RenderFeature` is the extension point for adding custom rendering work to the engine. Features inject one or more graph nodes into the per-frame render graph without modifying engine code.

Use features for: post-processing effects, debug overlays, screen-space effects, compute dispatches, or any rendering work that reads/writes render targets. Engine-owned work (opaque forward, debug, composition) uses the same `RenderFeature` type.

## Registration vs execution (each frame)

**Who runs `AddPasses`, and in what order**

1. **`RenderOrchestrator::Prepare`** — validates views and **frame layers**, fills view matrices / frustums, culls and sorts scene submissions, and supplies **`FrameRenderParams::PrimaryView`** via `ResolvePreparedPrimaryView` from the renderer.
2. **Unified pass list** — every registered feature (engine and game), ordered by **`(RenderPhase, order, registration sequence)`**. Phases run from **`PreOpaque`** through **`Present`** (see `RenderOrchestrator.h`). Built-ins: **`SceneOpaquePass`** at **`Opaque`**, **`DebugPass`** at **`Overlay`**, **`CompositionPass`** at **`Present`**.

**What order the GPU runs**

After all graph nodes are recorded, **`RenderGraph::Compile`** topologically sorts passes using **resource dependencies** (`ReadTexture`, `WriteColour` with `LoadOp::Load`, `WriteDepth` with `LoadOp::Load`, etc.). Where dependencies do not constrain order, the sort is stable but you should not rely on incidental ordering—declare reads/writes explicitly.

**Compile validation:** a successful compile **requires at least one** pass that calls **`SetSwapchainOutput`**. In non-`NDEBUG` builds, **more than one** swapchain writer emits a **warning** (pipelines normally use exactly one present pass).

**Registration API**

| API | Use for |
|-----|---------|
| **`Renderer::AddPass(phase, order, pass)`** | Game / editor features — pick **`RenderPhase`** and optional ordering within the phase. |
| **`Renderer::AddPass(phase, pass)`** | Same with **`order` = 0**. |

All features live in one list; there is no separate "game segment". Passes registered before `Renderer::Initialise` are deferred and flushed automatically when the orchestrator initialises.

## `FrameRenderParams`

Passed to every `AddPasses`:

| Field | Contract |
|-------|----------|
| `Frame` | The prepared `RenderFrame` (views, **layers**, lights, **per-view volume effect stacks**). |
| `SwapchainWidth` / `SwapchainHeight` | Current swapchain extent used for transient targets; non-zero when rendering. |
| `BuiltInMeshes` | Fixed-size `BuiltInMeshTable` mapping `BuiltInMeshId` → `const Mesh*` for built-in primitives. |
| `ResourceCache` | May be null if asset resolution is unavailable; passes must tolerate null if they depend on it. |
| `PrimaryView` | Result of `ResolvePreparedPrimaryView(Frame)` — primary view matrices and clear colour when the first view is prepared. |

## `RenderPhase` (summary)

| Phase | Typical use |
|-------|----------------|
| `PreOpaque` | Shadow maps, G-buffer prep, hi-Z |
| `Opaque` | Main scene geometry |
| `PostOpaque` | SSAO, SSR, decals |
| `Transparent` | Alpha-blended geometry, particles |
| `PostProcess` | Bloom, DoF, game post chain |
| `Composite` | FXAA/TAA, optional handoff copies |
| `Overlay` | Debug, gizmos, UI |
| `Present` | Tonemap + swapchain (exactly one writer in typical pipelines) |

After **`Renderer::Initialise`**, optional modules register features with **`AddPass(phase, order, pass)`**. Features may also be registered before `Initialise` — they are deferred and attached once the orchestrator has a context.

## Capabilities

Inside **`graph.AddPass`** / **`AddComputePass`** setup, call **`builder.DeclarePassCapabilities(mask)`** when the graph node should be checked in **non-`NDEBUG`** `Compile` (e.g. scene geometry must attach colour or depth; overlay/debug must attach colour; fullscreen composite must call `SetSwapchainOutput`). Omit for passes that do not need these checks.

## Creating a Feature

Subclass `RenderFeature` and implement the required virtual methods:

```cpp
#include "rendering/graph/RenderFeature.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/pipeline/FrameRenderParams.h"

namespace Wayfinder
{
    class BloomPass : public RenderFeature
    {
    public:
        std::string_view GetName() const override
        {
            return "Bloom";
        }

        void AddPasses(RenderGraph& graph, const FrameRenderParams& params) override
        {
            graph.AddPass("BloomDownsample", [&](RenderGraphBuilder& builder)
            {
                auto sceneColour = graph.FindHandle(GraphTextureId::SceneColour);
                builder.ReadTexture(sceneColour);

                auto halfRes = builder.CreateTransient({
                    .Width = params.SwapchainWidth / 2,
                    .Height = params.SwapchainHeight / 2,
                    .Format = TextureFormat::RGBA16_FLOAT,
                    .DebugName = "BloomHalfRes",
                });
                builder.WriteColour(halfRes);
                return [](RenderDevice&, const RenderGraphResources&)
                {
                };
            });
        }

        void OnAttach(const RenderFeatureContext& ctx) override
        {
            (void)ctx.Context;
            // Register shader programs, create pipelines, etc.
        }

        void OnDetach(const RenderFeatureContext& ctx) override
        {
            (void)ctx.Context;
            // Release GPU resources.
        }
    };
}
```

Prefer **`graph.FindHandleChecked(GraphTextureId::…)`** or **`FindHandleChecked(name)`** when the resource must exist — it logs and **asserts in non-`NDEBUG` builds** if the handle is missing. **`ReadTexture` / `WriteColour` / `WriteDepth`** log an **error** if given an invalid handle (e.g. wrong `FindHandle` result) and skip the dependency edge.

## Registering a feature

```cpp
renderer.AddPass(Wayfinder::RenderPhase::PostProcess, 0, std::make_unique<BloomPass>());
```

Features registered earlier in the same **`(phase, order)`** run first (tie-break: registration order). The render graph still determines **GPU** order from resource dependencies where that differs.

## Enabling / Disabling

Features can be toggled at runtime without removal:

```cpp
auto* bloom = renderer.GetPass<BloomPass>();
if (bloom) bloom->SetEnabled(false);
```

Disabled features have their `AddPasses` skipped entirely — no graph nodes are created.

## Removing a Feature

```cpp
renderer.RemovePass<BloomPass>();
```

`OnDetach` is called before the feature is destroyed, giving it an opportunity to release GPU resources.

## Key Concepts

### Resource Dependencies

Features declare what they read and write through `RenderGraphBuilder`. The graph uses these declarations to determine execution order (topological sort) and to cull dead passes that produce resources nobody reads.

### Engine graph textures (`GraphTextureId`)

Stable **engine** colour/depth targets use `GraphTextureId` with singleton **`InternedString`** names in **`GraphTextures`**, plus **`GraphTextureIntern`** / **`GraphTextureName`** for lookups and debug names (`RenderGraph.h`):

| Id | `GraphTextures::*` | Role |
|----|--------------------|------|
| `GraphTextureId::SceneColour` | `GraphTextures::SceneColour` | Main scene colour (created by the MainScene graph pass). |
| `GraphTextureId::SceneDepth` | `GraphTextures::SceneDepth` | Main scene depth. |
| `GraphTextureId::PostProcessColour` | `GraphTextures::PostProcessColour` | Latest post-process output; **`FindHandle`** / **`ImportTexture`** return the **most recently added** resource with that name so features can chain. |

**`ResolvePostProcessInput`** (`rendering/graph/PostProcessUtils.h`) returns **`PostProcessColour`** when present, otherwise **`SceneColour`** — use it when sampling input for a post chain or for **`CompositionPass`**.

**`GraphTextureIntern(GraphTextureId)`** returns the shared `InternedString` for that id (no re-interning). **`GraphTextureName`** returns a **`std::string_view`** over that interned text. **`RenderGraphTextureDesc::DebugName`** is a **`std::string_view`** (assigned from literals, `GraphTextureName`, or other stable views before `CreateTransient` interns it).

Use **`graph.FindHandle(GraphTextureId::SceneColour)`** (or `FindHandleChecked`) after those resources exist in the graph. Arbitrary transient textures still use string names on `RenderGraphTextureDesc::DebugName` and `FindHandle("…")`.

### Post-processing data

Scene **`BlendableEffectVolumeComponent`** data is blended into **`RenderView::PostProcess`** (`BlendableEffectStack`) during extraction. Effect types are registered at runtime in **`BlendableEffectRegistry`** (engine defaults: `colour_grading`, `vignette`, `chromatic_aberration`). Each effect uses a small **typed** payload struct with **`Override<T>`** fields so authors can override individual parameters. The registry owns **identity, blend (Lerp), and JSON (de)serialisation** via **`BlendableEffectTraits<T>`** (see `volumes/BlendableEffectTraits.h`). Effect structs declare a `static constexpr auto FIELDS` tuple of **`FieldDesc`** entries (see `volumes/OverrideReflection.h`); the default trait specialisation derives all four operations from those fields. Explicit trait specialisations handle edge cases like legacy JSON keys.

The registry is injected via `RenderServices` (constructor DI) for rendering and extraction paths. **`CompositionPass`** reads **`ColourGradingParams`**, **`VignetteParams`**, and **`ChromaticAberrationParams`** from the primary view's stack (via **`EngineEffectIds`** on **`RenderServices`**) and uploads a **`CompositionUBO`** for **`composition.frag`**. Game code can register additional blendable types and consume them from the stack in custom features.

### Transient Resources

Use `builder.CreateTransient(desc)` for intermediate textures that only live for one frame. The engine's `TransientResourcePool` reuses GPU allocations across frames to minimize allocation overhead.

### RenderFeatureContext

`OnAttach` and `OnDetach` receive a `RenderFeatureContext` with a reference to the full `RenderServices` (`Context`). Use `ctx.Context` to reach `GetDevice()`, `GetPrograms()`, `GetShaders()`, `GetPipelines()`, `GetNearestSampler()`, transient buffers, and other shared infrastructure.

### Multi-view

Split-screen / multiple render targets are not fully specified here: either **one graph per view** or **view-indexed resources** should be chosen explicitly before implying a single shared handoff texture for all views.

### Error policy (pipelines / resources)

- **Development / non-`WAYFINDER_SHIPPING`:** Missing shaders or pipelines for **engine** paths should surface as **errors** where practical (`FindHandleChecked`, composition path). Game features should log clearly when optional GPU state is missing.
- **Shipping (`WAYFINDER_SHIPPING`):** Prefer **structured warnings** and **skipping** draws rather than crashing; avoid silent failure without at least one log line for unexpected missing programs.

## Testing

Features can be tested headlessly using `NullDevice`, which implements the full `RenderDevice` interface as no-ops. See `tests/rendering/RenderFeatureTests.cpp` for examples.
