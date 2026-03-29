---
name: Pluggable Composition Refactor
overview: Decompose the monolithic CompositionPass into three self-contained RenderFeatures — ChromaticAberration, Vignette, ColourGrading — each owning its own effect registration, shader, and render graph pass. CompositionPass becomes a pure swapchain blit. All centralised scaffolding deleted.
todos:
  - id: post-process-helpers
    content: Add CreatePostProcessOutput helper and generic ResolveEffect<T> template
    status: completed
  - id: registry-accessor
    content: Add GetBlendableEffectRegistry() accessor to RenderServices
    status: completed
  - id: ca-feature
    content: Create ChromaticAberrationFeature (header + cpp + chromatic_aberration.frag)
    status: completed
  - id: vignette-feature
    content: Create VignetteFeature (header + cpp + vignette.frag)
    status: completed
  - id: colour-grading-feature
    content: Create ColourGradingFeature (header + cpp + colour_grading.frag)
    status: pending
  - id: simplify-composition
    content: Simplify CompositionPass to pure swapchain blit
    status: completed
  - id: self-register-shaders
    content: Move material shader registrations into SceneOpaquePass::OnAttach; delete BuiltInShaderPrograms.h/.cpp
    status: completed
  - id: delete-scaffolding
    content: Delete EngineEffectIds, EngineBlendableEffectNames, RegisterEngineBlendableEffects, Resolve*ForView, CompositionUBO
    status: completed
  - id: update-orchestrator
    content: Update RenderOrchestrator::Initialise --- register new features, remove old bootstrapping
    status: completed
  - id: update-fallback-validation
    content: Remove ENGINE_DEFAULT_BLENDABLE_EFFECT_NAMES fallback from BlendableEffect.cpp and ComponentRegistry.cpp
    status: completed
  - id: update-phase-docs
    content: Update RenderPhase enum comments (Present = blit, Composite = tonemapping + colour grading)
    status: completed
  - id: update-cmake
    content: Update CMakeLists.txt --- add new files, remove deleted files
    status: completed
  - id: compile-shaders
    content: Compile new .frag shaders to SPIR-V; delete composition.frag
    status: completed
  - id: tests
    content: Tests for ChromaticAberrationFeature, VignetteFeature, ColourGradingFeature, updated orchestrator/blendable tests
    status: completed
isProject: false
---

# Pluggable Composition Refactor

## Problem

The blendable effect registry and pass pipeline are generic and composable, but the composition pass is a monolith. Adding a new post-process effect (e.g. bloom, DOF) still requires touching **six places**: `EngineBlendableEffectNames`, `EngineEffectIds`, `RegisterEngineBlendableEffects`, `CompositionPass`, `CompositionUBO`, and `composition.frag`. The same anti-pattern repeats with `RegisterBuiltInShaderPrograms` --- a central C++ list for something that should be owned by its consumers.

## Architecture

Each post-process effect becomes a self-contained `RenderFeature` owning its effect registration, shader program, UBO, and render graph pass. No centralised scaffolding. No effect knows about any other effect.

```mermaid
flowchart LR
  subgraph PostProcess ["PostProcess Phase"]
    CA["ChromaticAberrationFeature"]
    Vig["VignetteFeature"]
  end
  subgraph Composite ["Composite Phase"]
    CG["ColourGradingFeature"]
  end
  subgraph Present ["Present Phase"]
    Comp["CompositionPass\n(pure swapchain blit)"]
  end

  SC["SceneColour"] --> CA
  CA -->|PostProcessColour| Vig
  Vig -->|PostProcessColour| CG
  CG -->|PostProcessColour| Comp
  Comp --> SW["Swapchain"]
```

`CompositionPass` shrinks to a trivial swapchain blit. It reuses `fullscreen_copy.frag` (already exists) --- no UBO, no effects knowledge.

### Why three standalone features, not a combined uber-shader

Earlier revisions of this plan combined vignette with colour grading in one "ColourCorrectionFeature" to save a fullscreen pass. That grouping is semantically wrong:

- **Colour grading** is a pure colour-space operation: tonemapping, LGG, contrast, saturation. It manipulates pixel values independently of screen position.
- **Vignette** is a spatial/lens simulation: it darkens pixels based on their UV distance from centre. It's conceptually closer to chromatic aberration (lens artifacts) than to colour grading.

Grouping them under "colour correction" misrepresents what each effect does. The savings from combining (~25% of one fullscreen pass, or ~9 µs at 640×480) don't justify the semantic confusion.

Each feature is fully self-contained: one UBO struct, one shader, one `AddPasses` method, no coupling to any other effect. Adding a new post-process effect means writing one new `RenderFeature` class and one shader — nothing else changes.

### Performance

The engine supports two rendering modes: **upscaled** (render at a lower internal resolution and upscale to display) and **native** (render and post-process at native display resolution). The post-process chain runs at whatever resolution the scene was rendered at.

**4-draw chain: CA → Vignette → Colour grading → Blit**

| Resolution | Pixels/pass | RGBA16F/transient | BW (read+write, 4 passes) | Est. time (500 GB/s GPU) |
|---|---|---|---|---|
| 640×480 (upscaled) | 307K | 2.4 MB | ~19 MB | ~37 µs |
| 1280×720 | 922K | 7.4 MB | ~59 MB | ~0.12 ms |
| 1920×1080 | 2.1M | 16.6 MB | ~133 MB | ~0.27 ms |
| 2560×1440 | 3.7M | 29.5 MB | ~236 MB | ~0.47 ms |
| 3840×2160 (4K) | 8.3M | 66 MB | ~528 MB | ~1.1 ms |

**Budget context:**

| Resolution | 60 fps budget | 4-pass chain | % of budget |
|---|---|---|---|
| 640×480 | 16.67 ms | ~37 µs | 0.2% |
| 1920×1080 | 16.67 ms | ~0.27 ms | 1.6% |
| 3840×2160 | 16.67 ms | ~1.1 ms | 6.6% |

At 1080p raw, the chain is ~1.6% of frame time. At 4K raw it's ~6.6% --- meaningful but acceptable, and the upscaled path makes it negligible regardless of display resolution.

**Future: combining per-pixel colour effects.** When the engine has multiple true colour-space effects that are semantically related (e.g. colour grading + film grain + night vision), combining them into one uber-shader with a shared UBO and enable bitmask will save one draw per merged effect. That's a worthwhile optimisation to pursue when there are multiple candidates — not with the current three-effect set where each effect has distinct characteristics.

---

## Step 1: Post-process helpers

**File:** [RenderGraph.h](engine/wayfinder/src/rendering/graph/RenderGraph.h)

### 1a. `CreatePostProcessOutput` helper

Every post-process feature that writes `PostProcessColour` must create a transient with the correct interned name and RGBA16F format. One typo silently breaks the chain. Add a helper next to `ResolvePostProcessInput`:

```cpp
/// Creates a new PostProcessColour transient for the next link in the chain.
/// Always RGBA16F to avoid banding between passes (pre-tonemapped HDR data).
inline RenderGraphHandle CreatePostProcessOutput(
    RenderGraphBuilder& builder, uint32_t width, uint32_t height)
{
    return builder.CreateTransient({
        .Width = width,
        .Height = height,
        .Format = TextureFormat::RGBA16F,
        .DebugName = GraphTextureName(GraphTextureId::PostProcessColour),
    });
}
```

### 1b. Generic `ResolveEffect<T>` template

Replace the three per-effect `Resolve*ForView` functions with one template. Each feature calls this directly --- no `RenderingEffects.h` coupling:

```cpp
/// Resolve a blendable effect payload from the stack, or return identity if absent.
template<BlendableEffectPayload T>
T ResolveEffect(const BlendableEffectStack& stack, BlendableEffectId id)
{
    const auto* p = stack.FindPayload<T>(id);
    return p ? *p : T{};
}
```

This lives in `volumes/BlendableEffectStack.h` (or a small `BlendableEffectUtils.h`).

---

## Step 2: Expose BlendableEffectRegistry via RenderServices

Features need the registry in `OnAttach` to self-register. Add a getter to [RenderServices.h](engine/wayfinder/src/rendering/pipeline/RenderServices.h):

```cpp
BlendableEffectRegistry* GetBlendableEffectRegistry() { return m_blendableEffectRegistry; }
```

**Sealing invariant:** Features register blendable effects during `OnAttach`, which runs inside `RenderOrchestrator::Initialise`. The registry auto-seals on the first `Renderer::Render()` call. No feature may register blendable effects after the first frame. This is already enforced by `BlendableEffectRegistry::Seal()` but should be documented in the `RenderFeature::OnAttach` Javadoc.

---

## Step 3: Create ChromaticAberrationFeature

**New files:**
- `engine/wayfinder/src/rendering/passes/ChromaticAberrationFeature.h`
- `engine/wayfinder/src/rendering/passes/ChromaticAberrationFeature.cpp`
- `engine/wayfinder/shaders/chromatic_aberration.frag`

- **Phase:** `PostProcess`, order 800.
- **OnAttach:** registers `ChromaticAberrationParams` with the `BlendableEffectRegistry`, registers a `"chromatic_aberration"` shader program.
- **AddPasses:** resolves `ChromaticAberrationParams` via `ResolveEffect<ChromaticAberrationParams>(stack, m_effectId)`. If intensity is zero, skips (no graph node). Otherwise reads `ResolvePostProcessInput(graph)`, creates `CreatePostProcessOutput(builder, w, h)`, applies RGB channel separation, writes it.
- **Shader (`chromatic_aberration.frag`):** extract CA math from `composition.frag` lines 32-39; UBO is a single `float4` (intensity + padding).

Stores `BlendableEffectId m_effectId` as a member (obtained from `BlendableEffectRegistry::Register` in `OnAttach`).

---

## Step 4: Create VignetteFeature

**New files:**
- `engine/wayfinder/src/rendering/passes/VignetteFeature.h`
- `engine/wayfinder/src/rendering/passes/VignetteFeature.cpp`
- `engine/wayfinder/shaders/vignette.frag`

Vignette is a spatial/lens simulation — it darkens pixels based on their UV distance from screen centre. It has nothing to do with colour-space manipulation. It gets its own feature and pass.

- **Phase:** `PostProcess`, order 900 (runs after CA).
- **OnAttach:** registers `VignetteParams` with the `BlendableEffectRegistry`, registers a `"vignette"` shader program.
- **AddPasses:** resolves `VignetteParams` via `ResolveEffect<VignetteParams>(stack, m_effectId)`. If strength is zero, skips (no graph node). Otherwise reads `ResolvePostProcessInput(graph)`, creates `CreatePostProcessOutput(builder, w, h)`, applies per-pixel darkening, writes it.

### Header

```cpp
class VignetteFeature final : public RenderFeature
{
public:
    std::string_view GetName() const override { return "Vignette"; }
    RenderCapabilityMask GetCapabilities() const override;

    void OnAttach(const RenderFeatureContext& context) override;
    void OnDetach(const RenderFeatureContext& context) override;
    void AddPasses(RenderGraph& graph, const FrameRenderParams& params) override;

private:
    RenderServices* m_context = nullptr;
    BlendableEffectId m_effectId = INVALID_BLENDABLE_EFFECT_ID;
};
```

### UBO struct

```cpp
/// Matches the `VignetteParams` cbuffer in `vignette.frag` exactly.
struct alignas(16) VignetteUBO
{
    float Strength = 0.0f;
    float _pad[3]{};
};
static_assert(sizeof(VignetteUBO) == 16);
```

### Shader (`vignette.frag`)

```hlsl
[[vk::binding(0, 3)]]
cbuffer VignetteParams : register(b0, space3) {
    float VignetteStrength;
    float3 _pad;
};

[[vk::binding(0, 2)]]
Texture2D<float4> SceneColourTex : register(t0, space2);
[[vk::binding(0, 2)]]
SamplerState PointSampler : register(s0, space2);

float4 PSMain(PSInput input) : SV_Target {
    float2 uv = input.TexCoord;
    float3 c = SceneColourTex.Sample(PointSampler, uv).rgb;

    float2 vigUv = uv * 2.0 - 1.0;
    c *= saturate(1.0 - dot(vigUv, vigUv) * VignetteStrength);

    return float4(c, 1.0);
}
```

---

## Step 5: Create ColourGradingFeature

**New files:**
- `engine/wayfinder/src/rendering/passes/ColourGradingFeature.h`
- `engine/wayfinder/src/rendering/passes/ColourGradingFeature.cpp`
- `engine/wayfinder/shaders/colour_grading.frag`

Colour grading is a pure colour-space operation: tonemapping, lift/gamma/gain, contrast, saturation. It always runs (identity params produce a passthrough; tonemapping must happen every frame).

- **Phase:** `Composite`, order 0.
- **OnAttach:** registers `ColourGradingParams` with the `BlendableEffectRegistry`, registers a `"colour_grading"` shader program.
- **AddPasses:** resolves `ColourGradingParams` via `ResolveEffect<ColourGradingParams>(stack, m_effectId)`. Always adds a graph node (grading identity is still a valid passthrough). Reads `ResolvePostProcessInput(graph)`, creates `CreatePostProcessOutput(builder, w, h)`, applies tonemapping + LGG + contrast + saturation, writes it.

### Header

```cpp
class ColourGradingFeature final : public RenderFeature
{
public:
    std::string_view GetName() const override { return "ColourGrading"; }
    RenderCapabilityMask GetCapabilities() const override;

    void OnAttach(const RenderFeatureContext& context) override;
    void OnDetach(const RenderFeatureContext& context) override;
    void AddPasses(RenderGraph& graph, const FrameRenderParams& params) override;

private:
    RenderServices* m_context = nullptr;
    BlendableEffectId m_effectId = INVALID_BLENDABLE_EFFECT_ID;
};
```

### UBO struct

```cpp
/// Matches the `ColourGradingParams` cbuffer in `colour_grading.frag` exactly.
struct alignas(16) ColourGradingUBO
{
    Float4 ExposureContrastSaturation{}; // x=exposure(stops), y=contrast, z=saturation, w=unused
    Float4 Lift{};
    Float4 Gamma{};
    Float4 Gain{};
};
static_assert(sizeof(ColourGradingUBO) == 64);
```

No enable bitmask needed — this feature always runs.

### AddPasses

```cpp
void ColourGradingFeature::AddPasses(RenderGraph& graph, const FrameRenderParams& params)
{
    const BlendableEffectStack* stack = /* params.Frame.Views.front().PostProcess */;
    auto grading = ResolveEffect<ColourGradingParams>(*stack, m_effectId);

    ColourGradingUBO ubo{};
    ubo.ExposureContrastSaturation = {
        grading.ExposureStops.Value(), grading.Contrast.Value(),
        grading.Saturation.Value(), 0.0f};
    ubo.Lift = {grading.Lift.Value().X, grading.Lift.Value().Y, grading.Lift.Value().Z, 0.0f};
    ubo.Gamma = {grading.Gamma.Value().X, grading.Gamma.Value().Y, grading.Gamma.Value().Z, 0.0f};
    ubo.Gain = {grading.Gain.Value().X, grading.Gain.Value().Y, grading.Gain.Value().Z, 0.0f};

    const auto input = ResolvePostProcessInput(graph);
    graph.AddPass("ColourGrading", [&](RenderGraphBuilder& builder) {
        builder.ReadTexture(input);
        auto output = CreatePostProcessOutput(builder, w, h);
        builder.WriteColour(output, LoadOp::DontCare);

        return [ubo, input, output, this](RenderDevice& device, const RenderGraphResources& res) {
            device.BindPipeline(/* colour_grading pipeline */);
            device.BindFragmentSampler(0, res.GetTexture(input), m_context->GetNearestSampler());
            device.PushFragmentUniform(0, &ubo, sizeof(ColourGradingUBO));
            device.DrawPrimitives(3);
        };
    });
}
```

### Shader (`colour_grading.frag`)

```hlsl
[[vk::binding(0, 3)]]
cbuffer ColourGradingParams : register(b0, space3) {
    float4 ExposureContrastSaturationPad;   // x=exposure(stops), y=contrast, z=saturation
    float4 Lift;
    float4 Gamma;
    float4 Gain;
};

[[vk::binding(0, 2)]]
Texture2D<float4> SceneColourTex : register(t0, space2);
[[vk::binding(0, 2)]]
SamplerState PointSampler : register(s0, space2);

float4 PSMain(PSInput input) : SV_Target {
    float2 uv = input.TexCoord;
    float3 c = SceneColourTex.Sample(PointSampler, uv).rgb;

    // Tonemapping (exposure in stops).
    c *= exp2(ExposureContrastSaturationPad.x);

    // Lift / Gamma / Gain.
    c += Lift.rgb;
    c *= Gain.rgb;
    c = pow(max(c, 1e-5), 1.0 / max(Gamma.rgb, 1e-5));

    // Contrast (around mid-grey).
    c = (c - 0.5) * ExposureContrastSaturationPad.y + 0.5;

    // Saturation.
    float luma = dot(c, float3(0.2126, 0.7152, 0.0722));
    c = lerp(float3(luma, luma, luma), c, ExposureContrastSaturationPad.z);

    return float4(c, 1.0);
}
```

---

## Step 6: Simplify CompositionPass to a swapchain blit

**Files:** [CompositionPass.h](engine/wayfinder/src/rendering/passes/CompositionPass.h), [CompositionPass.cpp](engine/wayfinder/src/rendering/passes/CompositionPass.cpp)

- `OnAttach` registers a `"composition_blit"` shader program using `fullscreen.vert` + `fullscreen_copy.frag` (both already exist).
- `AddPasses` reads `ResolvePostProcessInput(graph)` and writes to swapchain. No UBO. No effect knowledge.
- Remove all references to `EngineEffectIds`, `ColourGradingParams`, `VignetteParams`, `ChromaticAberrationParams`, `CompositionUBO`, `MakeCompositionUBO`.

---

## Step 7: Self-register shader programs per feature

**Files:** [BuiltInShaderPrograms.h](engine/wayfinder/src/rendering/pipeline/BuiltInShaderPrograms.h), [BuiltInShaderPrograms.cpp](engine/wayfinder/src/rendering/pipeline/BuiltInShaderPrograms.cpp)

Move the material shader program descriptions (unlit, unlit_blended, basic_lit, textured_lit) into [SceneOpaquePass.cpp](engine/wayfinder/src/rendering/passes/SceneOpaquePass.cpp) `OnAttach`. The `ShaderProgramRegistry::Register` already skips duplicates, so if a future TransparentPass also needs `"unlit"`, it can register it again idempotently.

Delete `BuiltInShaderPrograms.h` and `BuiltInShaderPrograms.cpp`. Remove the include and call from [RenderOrchestrator.cpp](engine/wayfinder/src/rendering/pipeline/RenderOrchestrator.cpp).

---

## Step 8: Delete centralised prototype scaffolding

### From [BlendableEffectRegistry.h](engine/wayfinder/src/volumes/BlendableEffectRegistry.h):
- Delete `EngineBlendableEffectNames` namespace
- Delete `ENGINE_DEFAULT_BLENDABLE_EFFECT_NAMES` array
- Delete `EngineEffectIds` struct

### From [RenderServices.h](engine/wayfinder/src/rendering/pipeline/RenderServices.h) / [RenderServices.cpp](engine/wayfinder/src/rendering/pipeline/RenderServices.cpp):
- Delete `RegisterEngineBlendableEffects()` declaration and implementation
- Delete `GetEngineEffectIds()` accessor
- Delete `m_engineEffectIds` member

### From [RenderingEffects.h](engine/wayfinder/src/rendering/materials/RenderingEffects.h) / [RenderingEffects.cpp](engine/wayfinder/src/rendering/materials/RenderingEffects.cpp):
- Delete `ResolveColourGradingForView`, `ResolveVignetteForView`, `ResolveChromaticAberrationForView`. Replaced by the generic `ResolveEffect<T>` template from Step 1b.

### From [BuiltInUBOs.h](engine/wayfinder/src/rendering/pipeline/BuiltInUBOs.h):
- Delete `CompositionUBO` struct and `MakeCompositionUBO` function. UBO structs now live in their respective feature headers.

### From [BlendableEffect.cpp](engine/wayfinder/src/volumes/BlendableEffect.cpp) and [ComponentRegistry.cpp](engine/wayfinder/src/scene/ComponentRegistry.cpp):
- Remove fallback validation against `ENGINE_DEFAULT_BLENDABLE_EFFECT_NAMES`. Replace with: if no registry is available, warn and accept any name (the registry is always available since `EngineRuntime::Initialise` sets it before any scene loading).

---

## Step 9: Update RenderOrchestrator

[RenderOrchestrator.cpp](engine/wayfinder/src/rendering/pipeline/RenderOrchestrator.cpp) `Initialise` becomes:

```cpp
// --- Scene ---
RegisterPass(RenderPhase::Opaque,      0,   std::make_unique<SceneOpaquePass>());

// --- Post-process (each owns its effect registration + shader) ---
RegisterPass(RenderPhase::PostProcess,  800, std::make_unique<ChromaticAberrationFeature>());
RegisterPass(RenderPhase::PostProcess,  900, std::make_unique<VignetteFeature>());

// --- Colour grading (tonemapping + LGG + contrast + saturation) ---
RegisterPass(RenderPhase::Composite,    0,   std::make_unique<ColourGradingFeature>());

// --- Overlay + present ---
RegisterPass(RenderPhase::Overlay,      0,   std::make_unique<DebugPass>());
RegisterPass(RenderPhase::Present,      0,   std::make_unique<CompositionPass>());
```

Remove `services.RegisterEngineBlendableEffects()` and `RegisterBuiltInShaderPrograms(...)` calls.

Adding a new post-process effect: `RegisterPass(PostProcess, N, make_unique<NewEffectFeature>())`. The new feature self-registers its blendable effect and shader — nothing else changes.

---

## Step 10: Update RenderPhase enum docs

**File:** [RenderOrchestrator.h](engine/wayfinder/src/rendering/pipeline/RenderOrchestrator.h)

The current comments no longer match. Update:

```cpp
PostProcess = 4,  // Per-pixel and spatial effects: chromatic aberration, vignette, bloom, DOF
Composite = 5,    // Colour-space transforms: tonemapping, colour grading
Present = 7,      // Pure swapchain blit (exactly one pass)
```

---

## Step 11: Delete composition.frag, compile new shaders

- Delete `composition.frag` (replaced by `vignette.frag`, `colour_grading.frag`, and the existing `fullscreen_copy.frag` for the blit).
- Create `chromatic_aberration.frag`, `vignette.frag`, and `colour_grading.frag`.
- Compile all to SPIR-V (DXC, same pattern as existing shaders).

---

## Step 12: Update CMakeLists.txt

### Add:
- `engine/wayfinder/src/rendering/passes/ChromaticAberrationFeature.h/.cpp`
- `engine/wayfinder/src/rendering/passes/VignetteFeature.h/.cpp`
- `engine/wayfinder/src/rendering/passes/ColourGradingFeature.h/.cpp`
- Shader files: `chromatic_aberration.frag`, `vignette.frag`, `colour_grading.frag`

### Remove:
- `engine/wayfinder/src/rendering/pipeline/BuiltInShaderPrograms.h/.cpp`
- `engine/wayfinder/shaders/composition.frag`

---

## Step 13: Tests

### New tests:

- **ChromaticAberrationFeature:**
  - `OnAttach` registers blendable effect + shader.
  - `AddPasses` produces a graph node when intensity > 0.
  - `AddPasses` produces no graph node when intensity is 0 (skipped).

- **VignetteFeature:**
  - `OnAttach` registers blendable effect + shader.
  - `AddPasses` produces a graph node when strength > 0.
  - `AddPasses` produces no graph node when strength is 0 (skipped).
  - `VignetteUBO` layout: `static_assert(sizeof(VignetteUBO) == 16)`.

- **ColourGradingFeature:**
  - `OnAttach` registers blendable effect + shader.
  - `AddPasses` always produces a graph node (grading always runs).
  - `ColourGradingUBO` fields match expected values given known params from the stack.
  - `static_assert(sizeof(ColourGradingUBO) == 64)` --- catches accidental layout drift.

### Updated tests:

- **CompositionPass:** reflect that it no longer resolves effects (pure blit).
- **RenderOrchestrator:** [RenderOrchestratorTests.cpp](tests/rendering/RenderOrchestratorTests.cpp) --- update default pass list.
- **SceneOpaquePass:** [SceneOpaquePassTests.cpp](tests/rendering/SceneOpaquePassTests.cpp) --- verify shader programs are registered in OnAttach.
- **Blendable validation:** update `BlendableEffectRegistryTests.cpp` to remove references to `EngineEffectIds` or `ENGINE_DEFAULT_BLENDABLE_EFFECT_NAMES`.

---

## Verification

1. `cmake --build --preset debug` --- clean build
2. `ctest --preset test` --- all tests pass
3. `tools/lint.py --changed` and `tools/tidy.py --changed` --- clean
4. Journey sandbox visual parity: same rendering output (the math in the new shaders is identical to the old composition.frag)

---

## Design Notes

### Why three features, not one uber-shader

The three current effects have distinct characteristics:

| Effect | Characteristic | Why standalone |
|---|---|---|
| Chromatic aberration | UV distortion (offset texture reads per channel) | Cannot share a pass — dependent reads at different UVs |
| Vignette | Spatial/lens simulation (UV-distance darkening) | Not a colour-space op; semantically a lens artifact |
| Colour grading | Pure colour-space transform (exposure, LGG, contrast, sat) | Could share with other colour-space ops, but is alone for now |

Combining things that aren't semantically related for a ~25% savings on one fullscreen pass is premature optimisation that hurts readability. Each feature is self-contained: one UBO, one shader, one `AddPasses`, zero coupling.

### Non-contiguous chain correctness

If CA runs but vignette skips (strength = 0), `ResolvePostProcessInput` in the colour grading pass returns CA's output. If CA also skips, it falls through to `SceneColour`. The chain handles gaps correctly because `FindHandle` always returns the latest writer --- no feature assumes a specific predecessor. Colour grading always runs (identity is still a valid passthrough; tonemapping must happen), so the chain is never interrupted after the Composite phase.

### Future: combining per-pixel colour effects

When the engine has multiple semantically-related colour-space effects (e.g. colour grading + film grain + night vision), combining them into one concrete feature with a shared UBO and enable bitmask will be worthwhile:

1. Write a `ColourPipelineFeature` that resolves all colour-space param sets and fills one contiguous UBO.
2. One `colour_pipeline.frag` uber-shader with bitmasked sections.
3. Each merged effect saves one full read+write cycle of the intermediate buffer.

This is the same concrete-combined-feature approach from the earlier plan revision — it's a good pattern when the effects genuinely belong together. It just doesn't apply to the current three-effect set.

### Extending the pipeline

To add a new post-process effect:
1. Write a `RenderFeature` subclass (header + cpp) with `OnAttach` (register blendable effect + shader), `AddPasses` (resolve params, fill UBO, fullscreen draw).
2. Write the `.frag` shader.
3. Register it in `RenderOrchestrator::Initialise` with the appropriate phase and order.
4. Add to CMakeLists.txt.

Nothing else changes. No central lists, no shared UBOs, no coordination with other effects.
