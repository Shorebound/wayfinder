# Render Features

## Purpose

A `RenderFeature` is the primary extension point for adding custom rendering work to the engine. Features inject one or more passes into the per-frame render graph without modifying engine code.

Use features for: post-processing effects, debug overlays, screen-space effects, compute dispatches, or any rendering work that reads/writes render targets.

## Creating a Feature

Subclass `RenderFeature` and implement the required virtual methods:

```cpp
#include "rendering/RenderFeature.h"
#include "rendering/RenderGraph.h"
#include "rendering/RenderFrame.h"

namespace Wayfinder
{
    class BloomFeature : public RenderFeature
    {
    public:
        const std::string& GetName() const override
        {
            static const std::string name = "Bloom";
            return name;
        }

        void AddPasses(RenderGraph& graph, const RenderFrame& frame) override
        {
            graph.AddPass("BloomDownsample", [&](RenderGraphBuilder& builder)
            {
                // Read the engine's main colour output.
                auto sceneColor = graph.Import(WellKnown::SceneColor, /* ... */);
                builder.ReadTexture(sceneColor);

                // Create a transient half-res target for the downsample.
                auto halfRes = builder.CreateTransient({
                    .Width  = frame.ScreenWidth / 2,
                    .Height = frame.ScreenHeight / 2,
                    .Format = TextureFormat::RGBA16_FLOAT,
                    .DebugName = "BloomHalfRes",
                });
                builder.WriteColor(halfRes);
            },
            [](const RenderGraphResources& resources, RenderDevice& device)
            {
                // Execute: bind textures, dispatch draw calls.
            });
        }

        void OnAttach(const RenderFeatureContext& ctx) override
        {
            // Register shader programs, create pipelines, etc.
        }

        void OnDetach(const RenderFeatureContext& ctx) override
        {
            // Release GPU resources.
        }
    };
}
```

## Registering a Feature

Register features with the `Renderer` after initialisation:

```cpp
renderer.AddFeature(std::make_unique<BloomFeature>());
```

Features are called after the engine's core passes (MainScene, Debug) during each frame. The render graph determines execution order from resource dependencies, not registration order.

## Enabling / Disabling

Features can be toggled at runtime without removal:

```cpp
auto* bloom = renderer.GetFeature<BloomFeature>();
if (bloom) bloom->SetEnabled(false);
```

Disabled features have their `AddPasses` skipped entirely — no graph nodes are created.

## Removing a Feature

```cpp
renderer.RemoveFeature<BloomFeature>();
```

`OnDetach` is called before the feature is destroyed, giving it an opportunity to release GPU resources.

## Key Concepts

### Resource Dependencies

Passes declare what they read and write through `RenderGraphBuilder`. The graph uses these declarations to determine execution order (topological sort) and to cull dead passes that produce resources nobody reads.

### Well-Known Resources

The engine publishes named resources that features can reference:

| Name | Description |
|------|-------------|
| `WellKnown::SceneColor` | Main scene colour target (written by MainScene pass) |
| `WellKnown::SceneDepth` | Main scene depth target (written by MainScene pass) |

Import these by name via `graph.Import(WellKnown::SceneColor, ...)` to read or write engine-owned targets.

### Transient Resources

Use `builder.CreateTransient(desc)` for intermediate textures that only live for one frame. The engine's `TransientResourcePool` reuses GPU allocations across frames to minimize allocation overhead.

### RenderFeatureContext

`OnAttach` and `OnDetach` receive a `RenderFeatureContext` providing access to:

- `Device` — the current `RenderDevice`
- `ProgramRegistry` — register shader programs
- `ShaderManager` — load/compile shaders
- `PipelineCache` — create or look up GPU pipelines
- `NearestSampler` — a pre-created nearest-filter sampler handle

## Testing

Features can be tested headlessly using `NullDevice`, which implements the full `RenderDevice` interface as no-ops. See `tests/rendering/RenderFeatureTests.cpp` for examples.
