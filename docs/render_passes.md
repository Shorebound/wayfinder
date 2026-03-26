# Render Passes

## Purpose

A `RenderPass` is the extension point for adding custom rendering work to the engine. Passes inject one or more graph nodes into the per-frame render graph without modifying engine code.

Use passes for: post-processing effects, debug overlays, screen-space effects, compute dispatches, or any rendering work that reads/writes render targets. Engine-owned work (MainScene, Debug, Composition) uses the same `RenderPass` type; the renderer keeps separate ownership lists for engine passes (fixed at init) and game passes (added/removed at runtime).

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
                auto sceneColour = graph.FindHandle(WellKnown::SceneColour);
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

## Registering a Pass

Register game passes with the `Renderer` after initialisation:

```cpp
renderer.AddPass(std::make_unique<BloomPass>());
```

Game passes run after the engine passes (MainScene, Debug) each frame. The render graph determines execution order from resource dependencies, not registration order.

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

### Well-Known Resources

The engine publishes named resources that passes can reference:

| Name | Description |
|------|-------------|
| `WellKnown::SceneColour` | Main scene colour target (written by MainScene pass) |
| `WellKnown::SceneDepth` | Main scene depth target (written by MainScene pass) |

Use `graph.FindHandle(WellKnown::SceneColour)` after those resources exist in the graph.

### Transient Resources

Use `builder.CreateTransient(desc)` for intermediate textures that only live for one frame. The engine's `TransientResourcePool` reuses GPU allocations across frames to minimize allocation overhead.

### RenderPassContext

`OnAttach` and `OnDetach` receive a `RenderPassContext` with a reference to the full `RenderContext` (`Context`). Use `ctx.Context` to reach `GetDevice()`, `GetPrograms()`, `GetShaders()`, `GetPipelines()`, `GetNearestSampler()`, transient buffers, and other shared infrastructure.

## Testing

Passes can be tested headlessly using `NullDevice`, which implements the full `RenderDevice` interface as no-ops. See `tests/rendering/RenderPassTests.cpp` for examples.
