# Render Passes

## Glossary (read this first)

| Term | Meaning |
|------|---------|
| **`FrameLayerRecord` / `RenderFrame::Layers`** | CPU-side per-view layer of mesh submissions and/or debug draws (`RenderFrame.h`). **Not** the graph injector. |
| **`RenderLayerId` / `RenderLayers::Main`** | Scene **sorting** layer (e.g. main vs overlay) on a `RenderMeshSubmission`. |
| **`FrameLayerId` / `FrameLayerIds::MainScene`** | Which **logical layer record** (main_scene, overlay_scene, debug, …). |
| **`RenderPass` (graph injector)** | Subclass of `rendering/graph/RenderPass.h` — registers `AddPasses` to inject **graph nodes**. Engine and game use the same type. |
| **Graph node** | One `RenderGraph::AddPass` / `AddComputePass` entry for a frame. |
| **`RenderPassCapabilities` / `DeclarePassCapabilities`** | Bitmask describing injector behaviour. Graph nodes call `RenderGraphBuilder::DeclarePassCapabilities` for dev-time checks in `RenderGraph::Compile`. |
| **`PreparedPrimaryView` / `ResolvePreparedPrimaryView`** | Primary view matrices and clear colour after `Prepare` (`rendering/graph/RenderFrameUtils.h`); bundled on `RenderPipelineFrameParams::PrimaryView`. |
| **Early engine segment** | Engine passes whose phase is **not** `LateEngine` — run **before** game injectors. |
| **Late engine segment** | Engine passes registered with `EngineRenderPhase::LateEngine` — run **after** game injectors (e.g. present-source copy, swapchain composition). |

See also: [Workspace guide](workspace_guide.md) for repo layout.

## Purpose

A `RenderPass` is the extension point for adding custom rendering work to the engine. Passes inject one or more graph nodes into the per-frame render graph without modifying engine code.

Use passes for: post-processing effects, debug overlays, screen-space effects, compute dispatches, or any rendering work that reads/writes render targets. Engine-owned work (opaque forward, debug, present-source copy, composition) uses the same `RenderPass` type.

## Registration vs execution (each frame)

**Who runs `AddPasses`, and in what order**

1. **`RenderPipeline::Prepare`** — validates views and **frame layers**, fills view matrices / frustums, culls and sorts scene submissions, and supplies **`RenderPipelineFrameParams::PrimaryView`** via `ResolvePreparedPrimaryView` from the renderer.
2. **Early engine injectors** — every registered engine pass **except** `EngineRenderPhase::LateEngine`, ordered by **`(EngineRenderPhase, orderWithinPhase, registration sequence)`**. Phases include `PreOpaque` → `OpaqueMain` → `PostOpaque` → `Debug` → `PreComposite`. Built-ins: opaque forward at `OpaqueMain`, debug at `Debug`.
3. **Game injectors** — each game pass in **`Renderer`’s** `m_passes` vector, in vector order.
4. **Late engine injectors** — passes registered with **`EngineRenderPhase::LateEngine`**, ordered by **`(orderWithinPhase, registration sequence)`**. Built-in: **`CompositionPass`** (samples `PresentSource` or `SceneColour`, writes swapchain). Optional: **`PresentSourceCopyPass`** (copies `SceneColour` → `PresentSource`) — not registered by default; activate via `RegisterEnginePass(LateEngine, …)` when a game needs a stable handoff texture.

**What order the GPU runs**

After all graph nodes are recorded, **`RenderGraph::Compile`** topologically sorts passes using **resource dependencies** (`ReadTexture`, `WriteColour` with `LoadOp::Load`, `WriteDepth` with `LoadOp::Load`, etc.). Where dependencies do not constrain order, the sort is stable but you should not rely on incidental ordering—declare reads/writes explicitly.

**Compile validation:** a successful compile **requires at least one** pass that calls **`SetSwapchainOutput`**. In non-`NDEBUG` builds, **more than one** swapchain writer emits a **warning** (pipelines normally use exactly one present pass).

**When to use which registration API**

| API | Use for |
|-----|---------|
| **`Renderer::RegisterEnginePass(phase, order, pass)`** | Engine modules: use **`LateEngine`** only when the pass must run **after** game `AddPasses` (e.g. final composite). |
| **`Renderer::AddPass(pass)`** | Game / sandbox code registering custom rendering in the **game** segment (between early and late engine). |

## `RenderPipelineFrameParams`

Passed to every `AddPasses`:

| Field | Contract |
|-------|----------|
| `Frame` | The prepared `RenderFrame` (views, **layers**, lights, **per-view volume effect stacks**). |
| `SwapchainWidth` / `SwapchainHeight` | Current swapchain extent used for transient targets; non-zero when rendering. |
| `MeshesByStride` | Map from vertex stride → `Mesh*` for built-in primitives used by passes that draw them. |
| `ResourceCache` | May be null if asset resolution is unavailable; passes must tolerate null if they depend on it. |
| `PrimaryView` | Result of `ResolvePreparedPrimaryView(Frame)` — primary view matrices and clear colour when the first view is prepared. |

## Engine pass registration

After `Renderer::Initialise`, optional engine modules may register injectors:

```cpp
renderer.RegisterEnginePass(Wayfinder::EngineRenderPhase::PostOpaque, 0, std::make_unique<MyEnginePass>());
renderer.RegisterEnginePass(Wayfinder::EngineRenderPhase::LateEngine, 0, std::make_unique<MyAfterGamePass>());
```

Requires pipeline initialisation (has `RenderContext`). **`LateEngine`** passes are stored separately and invoked **after** game passes.

## Capabilities

Inside **`graph.AddPass`** / **`AddComputePass`** setup, call **`builder.DeclarePassCapabilities(mask)`** when the graph node should be checked in **non-`NDEBUG`** `Compile` (e.g. scene geometry must attach colour or depth; overlay/debug must attach colour; fullscreen composite must call `SetSwapchainOutput`). Omit for passes that do not need these checks.

## Creating a Pass

Subclass `RenderPass` and implement the required virtual methods:

```cpp
#include "rendering/graph/RenderPass.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/pipeline/RenderPipelineFrameParams.h"

namespace Wayfinder
{
    class BloomPass : public RenderPass
    {
    public:
        std::string_view GetName() const override
        {
            return "Bloom";
        }

        void AddPasses(RenderGraph& graph, const RenderPipelineFrameParams& params) override
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

        void OnAttach(const RenderPassContext& ctx) override
        {
            (void)ctx.Context;
            // Register shader programs, create pipelines, etc.
        }

        void OnDetach(const RenderPassContext& ctx) override
        {
            (void)ctx.Context;
            // Release GPU resources.
        }
    };
}
```

Prefer **`graph.FindHandleChecked(GraphTextureId::…)`** or **`FindHandleChecked(name)`** when the resource must exist — it logs and **asserts in non-`NDEBUG` builds** if the handle is missing. **`ReadTexture` / `WriteColour` / `WriteDepth`** log an **error** if given an invalid handle (e.g. wrong `FindHandle` result) and skip the dependency edge.

## Registering a game pass

Register game passes with the `Renderer` after initialisation:

```cpp
renderer.AddPass(std::make_unique<BloomPass>());
```

Game `AddPasses` run **after** early engine injectors and **before** late engine injectors; the render graph still determines **GPU** order from resource dependencies.

## Enabling / Disabling

Passes can be toggled at runtime without removal:

```cpp
auto* bloom = renderer.GetPass<BloomPass>();
if (bloom) bloom->SetEnabled(false);
```

Disabled passes have their `AddPasses` skipped entirely — no graph nodes are created.

## Removing a Pass

```cpp
renderer.RemovePass<BloomPass>();
```

`OnDetach` is called before the pass is destroyed, giving it an opportunity to release GPU resources.

## Key Concepts

### Resource Dependencies

Passes declare what they read and write through `RenderGraphBuilder`. The graph uses these declarations to determine execution order (topological sort) and to cull dead passes that produce resources nobody reads.

### Engine graph textures (`GraphTextureId`)

Stable **engine** colour/depth/present targets use `GraphTextureId` with singleton **`InternedString`** names in **`GraphTextures`** (same pattern as `FrameLayerIds`), plus **`GraphTextureIntern`** / **`GraphTextureName`** for lookups and debug names (`RenderGraph.h`):

| Id | `GraphTextures::*` | Role |
|----|--------------------|------|
| `GraphTextureId::SceneColour` | `GraphTextures::SceneColour` | Main scene colour (created by the MainScene graph pass). |
| `GraphTextureId::SceneDepth` | `GraphTextures::SceneDepth` | Main scene depth. |
| `GraphTextureId::PresentSource` | `GraphTextures::PresentSource` | Colour buffer sampled by **`CompositionPass`** after optional post work; **`PresentSourceCopyPass`** fills it from `SceneColour` when no earlier pass writes it. |

**`GraphTextureIntern(GraphTextureId)`** returns the shared `InternedString` for that id (no re-interning). **`GraphTextureName`** returns a **`std::string_view`** over that interned text. **`RenderGraphTextureDesc::DebugName`** is a **`std::string_view`** (assigned from literals, `GraphTextureName`, or other stable views before `CreateTransient` interns it).

Use **`graph.FindHandle(GraphTextureId::SceneColour)`** (or `FindHandleChecked`) after those resources exist in the graph. Arbitrary transient textures still use string names on `RenderGraphTextureDesc::DebugName` and `FindHandle("…")`.

### Post-processing data

Scene **`BlendableEffectVolumeComponent`** data is blended into **`RenderView::PostProcess`** (`BlendableEffectStack`) during extraction. Effect types are registered at runtime in **`BlendableEffectRegistry`** (engine defaults: `colour_grading`, `vignette`, `chromatic_aberration`). Each effect uses a small **typed** payload struct with **`Override<T>`** fields so authors can override individual parameters. The registry owns **identity, blend (Lerp), and JSON (de)serialisation**; it does **not** add **`RenderPass`** graph nodes. **`CompositionPass`** reads **`ColourGradingParams`**, **`VignetteParams`**, and **`ChromaticAberrationParams`** from the primary view’s stack (via **`EngineEffectIds`** on **`RenderContext`**) and uploads a **`CompositionUBO`** for **`composition.frag`**. Game code can register additional blendable types and consume them from the stack in custom passes.

**`BlendableEffectRegistry::SetActiveInstance`** is set from **`EngineRuntime::Initialise`** (and used during **`RenderContext::RegisterEngineBlendableEffects`**) so scene load and validation resolve JSON **`type`** strings against the same registry.

### Transient Resources

Use `builder.CreateTransient(desc)` for intermediate textures that only live for one frame. The engine's `TransientResourcePool` reuses GPU allocations across frames to minimize allocation overhead.

### RenderPassContext

`OnAttach` and `OnDetach` receive a `RenderPassContext` with a reference to the full `RenderContext` (`Context`). Use `ctx.Context` to reach `GetDevice()`, `GetPrograms()`, `GetShaders()`, `GetPipelines()`, `GetNearestSampler()`, transient buffers, and other shared infrastructure.

### Multi-view

Split-screen / multiple render targets are not fully specified here: either **one graph per view** or **view-indexed resources** should be chosen explicitly before implying a single `PresentSource` for all views.

### Error policy (pipelines / resources)

- **Development / non-`WAYFINDER_SHIPPING`:** Missing shaders or pipelines for **engine** paths should surface as **errors** where practical (`FindHandleChecked`, composition path). Game passes should log clearly when optional GPU state is missing.
- **Shipping (`WAYFINDER_SHIPPING`):** Prefer **structured warnings** and **skipping** draws rather than crashing; avoid silent failure without at least one log line for unexpected missing programs.

## Testing

Passes can be tested headlessly using `NullDevice`, which implements the full `RenderDevice` interface as no-ops. See `tests/rendering/RenderPassTests.cpp` for examples.
