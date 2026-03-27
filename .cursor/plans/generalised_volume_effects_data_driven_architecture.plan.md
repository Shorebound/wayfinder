# Plan: Generalised Volume Effects, Unified Render Pass Pipeline, and Data-Driven Architecture

## TL;DR

Three interlocking systems, built in four phases:

1. **Phase 0 (immediate):** Generalise the blendable effect system — rename from PostProcess to Volume/Effect, relocate to its own domain, so it can serve rendering, audio, camera, physics, gameplay equally.
2. **Phase A (immediate):** Unify the render pass pipeline — replace early/game/late bands with a single phase-ordered pass list, introduce the `PostProcessChain` resource convention.
3. **Phase B (near-term):** Effect schema files — TOML metadata per effect type for editor UI, validation, and documentation.
4. **Phase C (future):** Data-driven pipeline configuration — TOML files controlling which passes are active, in what order, with hot-reload.

These compose into a system where: you define a struct, register it, write a consumer (render pass, audio system, camera controller, etc.), and optionally describe it in a schema file. Everything else — blending, serialisation, editor UI, validation, pass ordering — is automatic.

---

## Phase 0: Generalise the Blendable Effect System

The current system is architecturally generic but named as if it only handles post-processing. Before adding non-rendering consumers, rename and relocate.

### 0.1 — Rename all types

| Current Name | New Name |
|---|---|
| `PostProcessRegistry` | `VolumeEffectRegistry` |
| `PostProcessEffectId` | `VolumeEffectId` |
| `PostProcessEffectDesc` | `VolumeEffectDesc` |
| `PostProcessTag<T>` | `EffectTag<T>` |
| `BlendablePostProcessEffect` (concept) | `BlendableEffect` |
| `PostProcessEffect` | `VolumeEffect` |
| `PostProcessStack` | `VolumeEffectStack` |
| `PostProcessVolumeComponent` | `VolumeComponent` |
| `PostProcessVolumeInstance` | `VolumeInstance` |
| `PostProcessVolumeShape` | `VolumeShape` |
| `BlendPostProcessVolumes` | `BlendVolumeEffects` |
| `EnginePostProcessIds` | `EngineEffectIds` |
| `INVALID_POST_PROCESS_EFFECT_ID` | `INVALID_VOLUME_EFFECT_ID` |
| `POST_PROCESS_EFFECT_PAYLOAD_CAPACITY` | `VOLUME_EFFECT_PAYLOAD_CAPACITY` |

The rendering-specific params (`ColourGradingParams`, `VignetteParams`, `ChromaticAberrationParams`) keep their names — they describe specific effects and are already well-named.

### 0.2 — Relocate to a new `volumes` domain

**Current location:** `engine/wayfinder/src/rendering/materials/`

**New location:** `engine/wayfinder/src/volumes/`

New files:
- `engine/wayfinder/src/volumes/VolumeEffectRegistry.h` (was `PostProcessRegistry.h`)
- `engine/wayfinder/src/volumes/VolumeEffectRegistry.cpp` (was `PostProcessRegistry.cpp`)
- `engine/wayfinder/src/volumes/VolumeEffectRegistry.inl` (was `PostProcessRegistry.inl`)
- `engine/wayfinder/src/volumes/VolumeEffect.h` — `VolumeEffect`, `VolumeEffectStack`, `VolumeShape`, `VolumeInstance`, `VolumeComponent`
- `engine/wayfinder/src/volumes/VolumeEffect.cpp` — `BlendVolumeEffects` and resolve helpers
- `engine/wayfinder/src/volumes/Override.h` (moved from `rendering/materials/Override.h`)

**Rendering-specific effect params stay in rendering:**
- `engine/wayfinder/src/rendering/materials/RenderingEffects.h` — `ColourGradingParams`, `VignetteParams`, `ChromaticAberrationParams` and their ADL functions (extracted from `PostProcessVolume.h`)
- `engine/wayfinder/src/rendering/materials/RenderingEffects.cpp` — ADL function implementations (extracted from `PostProcessVolume.cpp`)

**Namespace:** `Wayfinder` (top-level, same as now). The volume system is engine-wide infrastructure, not rendering-specific. No sub-namespace needed.

### 0.3 — Update all consumers (17 files)

Every file that referenced the old names gets updated. The changes are purely mechanical (find-replace on type names, update include paths):

**Core definitions (replaced by new files):**
- `rendering/materials/PostProcessRegistry.h/.cpp/.inl` → deleted, replaced by `volumes/VolumeEffectRegistry.h/.cpp/.inl`
- `rendering/materials/PostProcessVolume.h/.cpp` → split into `volumes/VolumeEffect.h/.cpp` + `rendering/materials/RenderingEffects.h/.cpp`
- `rendering/materials/Override.h` → moved to `volumes/Override.h`

**Rendering pipeline (include path + type name updates):**
- `rendering/graph/RenderFrame.h` — `PostProcessStack` → `VolumeEffectStack`
- `rendering/pipeline/RenderContext.h/.cpp` — `PostProcessRegistry` → `VolumeEffectRegistry`, `EnginePostProcessIds` → `EngineEffectIds`
- `rendering/pipeline/SceneRenderExtractor.cpp` — `BlendPostProcessVolumes` → `BlendVolumeEffects`
- `rendering/pipeline/CompositionUBOUtils.h` — include path change only
- `rendering/passes/CompositionPass.cpp` — `PostProcessStack` → `VolumeEffectStack`, `EnginePostProcessIds` → `EngineEffectIds`

**Scene/ECS:**
- `scene/ComponentRegistry.cpp` — `PostProcessVolumeComponent` → `VolumeComponent`, registry accessor rename
- `scene/Components.h` — component type rename

**Tests:**
- `tests/rendering/PostProcessRegistryTests.cpp` → rename to `tests/core/VolumeEffectRegistryTests.cpp` (or `tests/volumes/`)
- `tests/rendering/RenderGraphTests.cpp` — type name updates

**Build + docs:**
- `engine/wayfinder/CMakeLists.txt` — update source file list
- `docs/render_passes.md` — update references

### 0.4 — Tests

Existing tests remain functionally identical after rename. Verify all pass with the new names.

---

## What the Volume Effect System Can Blend

### The mechanism

Any type that satisfies:
```
concept BlendableEffect = requires {
    Identity(EffectTag<T>{}) -> T;           // default state
    Lerp(const T&, const T&, float) -> T;    // per-field sparse blend
    Deserialise(EffectTag<T>{}, json) -> T;  // load from data
    Serialise(json&, const T&);              // save to data
};
```

Each field uses `Override<T>` for per-field sparse overrides. Volumes have shape (global/box/sphere), priority, blend distance, and weight. The system blends by priority, weighted by distance, and only touches fields where `Active == true`.

### What blends well (continuous numeric fields)

**Rendering — Post-processing (already implemented):**
- `ColourGradingParams` — exposure, contrast, saturation, lift/gamma/gain
- `VignetteParams` — strength
- `ChromaticAberrationParams` — intensity

**Rendering — Future post-processing (register + write a RenderPass):**
- Bloom — threshold, intensity, scatter, radius
- Depth of field — focus distance, focus range, bokeh size, aperture shape
- Motion blur — intensity, sample count, velocity scale
- Film grain — intensity, response curve, luminance contribution
- Lens flare — intensity, threshold, ghost count, chromatic spread
- Auto-exposure — min/max EV, adaptation speed, compensation

**Rendering — Environment and lighting:**
- Fog / atmosphere — density, height falloff, colour, scatter intensity, start distance, max distance
- Ambient light — colour, intensity, indirect bounce, sky contribution
- Shadow control — max distance, cascade splits, strength, bias, softness
- Volumetric lighting — density, scattering, anisotropy, absorption colour

**Camera:**
- FOV override — multiplier for tension/distortion
- Camera shake — intensity, frequency, decay
- Lens distortion — barrel/pincushion amount
- Motion blur scale — per-zone artistic override
- Depth of field focus hint — override focus distance per-zone (cave entrance sharpens interior)

**Audio:**
- Reverb — wet/dry mix, decay time, pre-delay, room size
- Low-pass / high-pass filter — cutoff frequency, resonance
- Ambient volume — multiplied with ambient sound bus
- Distance attenuation override — minimum/maximum hearing distance per-zone
- Occlusion strength — how much geometry dampens sound in this volume

**Physics / Environment:**
- Wind — direction, strength, turbulence, gust frequency (read by vegetation, particles, cloth, flags)
- Gravity override — direction, magnitude (underwater sections, zero-G zones)
- Drag / buoyancy — fluid density, drag coefficient

**Gameplay / Game State:**
- Time dilation — time scale, gravity multiplier (slow-motion zones)
- Danger level — 0.0–1.0 intensity used for music, UI vignette, AI alertness (abstract but useful)
- Interaction range multiplier — scaling pickup/interact distances per-zone

### What requires snap semantics (discrete values)

These work but use threshold-snap in the `Lerp` function rather than linear interpolation:

```
Override<int> RainType;  // 0=None, 1=Light, 2=Heavy, 3=Storm
// In Lerp: LerpOverride(current.RainType, source.RainType, t,
//     [](int a, int b, float w) { return w > 0.5f ? b : a; });
```

- Weather preset selection (enum/int)
- Music track ID / ambience zone ID
- Lighting scenario preset
- Any categorical parameter

### What does NOT fit

- **Variable-length data.** Arrays, lists, spline control points, animation curves. The SBO payload is fixed-size (96 bytes). Effects must be flat fixed-size structs.
- **Asset references that need blending.** You can store a texture/mesh ID in an `Override<uint32_t>` but you can't meaningfully lerp between two textures. Crossfading probes (e.g. environment cubemap) would store two IDs + a blend weight as separate fields — the consumer handles the crossfade.
- **Hierarchical/conditional overrides.** No "only override gamma if contrast is also overridden." Each field is independent. This is the same trade-off as Unity's volume system.
- **Multi-frame work.** Lightmap baking, texture streaming, shader warm-up — things that span frames. Volumes are per-frame spatial queries.
- **CPU-bound simulation.** Physics stepping, pathfinding, AI decision trees. These are systems, not parameter sets. Volumes can *influence* them (wind strength, gravity direction), but the systems themselves don't fit the "small struct of Override<T> fields" model.

### How a consumer reads blended data

**Rendering (via RenderPass in AddPasses):**
```
const BloomParams* bloom = stack.FindPayload<BloomParams>(ids.Bloom);
if (!bloom || bloom->Intensity.Value <= 0.0f) return;
// Upload UBO, draw, etc.
```

**Non-rendering (via VolumeEffectStack on a per-frame query):**
```
// Camera system, once per frame:
const CameraOverrideParams* cam = stack.FindPayload<CameraOverrideParams>(ids.CameraOverride);
if (cam) {
    currentFOV *= cam->FOVMultiplier.Value;
    shakeIntensity = cam->ShakeIntensity.Value;
}
```

**Audio (via its own blend query or shared stack):**
```
const AudioEnvironmentParams* audio = stack.FindPayload<AudioEnvironmentParams>(ids.AudioEnv);
if (audio) {
    reverbBus.SetWetDry(audio->ReverbWet.Value);
    lowPassFilter.SetCutoff(audio->LowPassCutoff.Value);
}
```

The volume system doesn't care who reads the data. It blends by position and produces a stack. Any system can query it.

---

## Phase A: Unified Render Pass Pipeline + PostProcessChain Convention

### What the RenderPass system handles

Everything that touches the GPU. The system is "something that injects graph nodes." Examples of passes at each phase:

**PreOpaque (phase 0):**
- `ShadowMapPass` — writes depth-only atlas/cascades, read by later passes
- `HiZPass` — compute pass that generates hierarchical-Z from previous frame depth (for occlusion culling)
- `GBufferPass` — (deferred) writes MRT: albedo, normals, roughness/metallic → used by deferred lighting

**Opaque (phase 1):**
- `SceneOpaquePass` — forward/deferred main scene geometry (already exists)
- `DeferredLightingPass` — reads GBuffer, writes scene colour (deferred path)
- `SkyboxPass` — renders skybox/atmosphere after opaque, before transparent

**PostOpaque (phase 2):**
- `SSAOPass` — reads scene depth + normals, writes AO texture
- `SSRPass` — reads depth + colour + normals, writes reflection texture
- `ScreenSpaceShadowsPass` — reads depth, writes shadow mask
- `DecalPass` — projected decals onto scene (blood splatter, bullet holes, graffiti)
- `LightClusteringPass` — compute pass: reads depth, writes clustered light list buffer

**Transparent (phase 3):**
- `TransparentPass` — alpha-blended geometry, reads scene depth
- `ParticleRenderPass` — GPU particles drawn after transparent geometry
- `VolumetricFogPass` — ray-marched volumetric scattering (reads depth, lights, fog params)
- `WaterSurfacePass` — reflections, refraction, caustics

**PostProcess (phase 4) — reads/writes PostProcessChain:**
- `BloomPass` — threshold + downsample chain + upsample composite
- `DepthOfFieldPass` — CoC calculation + blur (reads depth)
- `MotionBlurPass` — velocity buffer + directional blur
- `FilmGrainPass` — noise overlay
- `RainDistortionPass` (game-specific) — screen-space rain ripples
- `DreamSequencePass` (game-specific) — radial blur + colour shift + chromatic aberration
- `UnderwaterPass` (game-specific) — caustic overlay + colour tint + distortion

**Composite (phase 5):**
- `FXAAPass` / `TAAPass` — anti-aliasing on the final image
- `PresentSourceCopyPass` — blit to stable handoff texture (optional)

**Overlay (phase 6):**
- `DebugPass` — grid, lines, boxes (already exists)
- `EditorGizmoPass` — selection outlines, transform handles, editor widgets
- `ImGuiPass` — immediate-mode UI
- `GameUIPass` — in-world or screen-space game UI

**Present (phase 7):**
- `CompositionPass` — tonemapping + colour grading + swapchain blit (already exists, exactly one per frame)

### Unconventional passes that work

- **GPU particle simulation** — `AddComputePass` that reads/writes particle buffer. Particles are updated on the GPU, then `ParticleRenderPass` draws them. Two passes from one `RenderPass::AddPasses` call.
- **Procedural texture generation** — compute pass that writes a noise texture or LUT per frame. Other passes read it (atmosphere, water, terrain).
- **Indirect draw argument building** — compute pass that generates draw-indirect buffers from visibility data. The opaque pass consumes them.
- **Skinning / animation compute** — compute pass that transforms vertices in a buffer. Draw passes read the transformed buffer.
- **Screen-space text rendering** — raster pass that draws SDF glyph quads, writes to overlay colour target.

### What does NOT work as a RenderPass

- **CPU-only work** — frustum culling, LOD selection, animation evaluation, physics. These run in `Prepare()` or before `BuildGraph`.
- **Multi-frame work** — lightmap baking, texture streaming, shader compilation. Background systems that produce resources passes consume.
- **Resource creation/destruction** — loading meshes, creating textures. Handled by `OnAttach`/`OnDetach`, not per-frame.
- **Async compute** — not modelled yet. Would need queue affinity hints on graph nodes. Not a `RenderPass` limitation — it's a `RenderGraph` extension.
- **Ray tracing** — SDL_GPU doesn't expose RT. When it does, `AddRayTracingPass` alongside `AddPass`/`AddComputePass`. Same `RenderPass` pattern.

### What needs to exist for all of this to function

**Already implemented:**
- `RenderGraph` with `AddPass`/`AddComputePass`, topological sort, dead pass culling ✓
- `TransientResourcePool` for per-frame texture management ✓
- `RenderPass` base class with `AddPasses`/`OnAttach`/`OnDetach`/`IsEnabled` ✓
- `VolumeEffectRegistry` (was PostProcessRegistry) blending/serialisation ✓
- `Override<T>` per-field sparse overrides ✓
- Shader compilation pipeline (HLSL → SPIR-V) ✓
- `ShaderProgramRegistry` for named programs ✓
- `NullDevice` for headless testing ✓

**Needs implementation (Phase A, this plan):**
- `RenderPhase` enum (replaces `EngineRenderPhase`) — unified pass ordering
- Unified pass list in `RenderPipeline` — single sorted vector
- `PostProcessChain` resource convention + `ResolvePostProcessInput` helper
- `FindHandle` reverse-scan fix — return latest resource with matching name

**Needs implementation (future, not in this plan):**
- **Buffer resources in the render graph.** Compute passes that produce structured buffers (particle data, indirect args, light lists) can't declare buffer dependencies yet. The passes work — you can `AddComputePass` and manually manage buffers — but the graph can't auto-sort or cull based on buffer reads/writes. Roadmap item in render graph docs.
- **Temporal resources / history buffers.** TAA and motion blur need the previous frame's colour/velocity. The graph is per-frame. Solutions: `ImportTexture` for persistent textures (already supported) or `MarkPersistent` to keep transients across frames.
- **MRT (multi-render-target) builder ergonomics.** Already supported (`WriteColour` with slot index) but no helper for the common "GBuffer with 3-4 targets" case.

### A.1 — Expand and rename `EngineRenderPhase` → `RenderPhase`

**File:** `engine/wayfinder/src/rendering/pipeline/RenderPipeline.h`

```
enum class RenderPhase : uint8_t
{
    PreOpaque       = 0,
    Opaque          = 1,
    PostOpaque      = 2,
    Transparent     = 3,
    PostProcess     = 4,
    Composite       = 5,
    Overlay         = 6,
    Present         = 7,
};
```

### A.2 — Unify pass storage in `RenderPipeline`

**Files:** `RenderPipeline.h`, `RenderPipeline.cpp`

Single `m_passes` vector of `PassSlot{Phase, Order, InsertSequence, Pass}`, sorted by `(Phase, Order, InsertSequence)`.

`RegisterPass(RenderPhase, int32_t order, unique_ptr<RenderPass>)` replaces `RegisterEnginePass`. `BuildGraph` is a single loop — no `gamePasses` span.

### A.3 — Update `Renderer` public API (breaking change)

**Files:** `Renderer.h`, `Renderer.cpp`

Remove `AddPass(unique_ptr<RenderPass>)`, `RegisterEnginePass(...)`, `m_passes` member.

New:
```
void AddPass(RenderPhase phase, int32_t order, std::unique_ptr<RenderPass> pass);
void AddPass(RenderPhase phase, std::unique_ptr<RenderPass> pass); // order = 0
```

`RemovePass<T>()` and `GetPass<T>()` search the pipeline's unified list.

### A.4 — Update engine pass registration

```
RegisterPass(RenderPhase::Opaque,   0, std::make_unique<SceneOpaquePass>());
RegisterPass(RenderPhase::Overlay,  0, std::make_unique<DebugPass>());
RegisterPass(RenderPhase::Present,  0, std::make_unique<CompositionPass>());
```

### A.5 — `PostProcessChain` resource convention

Add `GraphTextureId::PostProcessChain` and `GraphTextures::PostProcessChain`.

`ResolvePostProcessInput(graph)` — returns `PostProcessChain` handle if any prior post-process pass wrote it, otherwise `SceneColour`.

Convention: post-process passes read via `ResolvePostProcessInput`, write to a new transient named `"PostProcessChain"`. Private intermediates use unique names.

### A.6 — CRITICAL: Fix `FindHandle` to return latest match

**File:** `RenderGraph.cpp`

Change `FindHandle` from forward scan to **reverse** scan. Currently returns the first match; must return the latest (most recently `CreateTransient`'d resource with that name). One-line change, backward-compatible (no existing code creates duplicate-named resources).

### A.7 — Update `CompositionPass`

Use `ResolvePostProcessInput(graph)` instead of `SceneColour`/`PresentSource` resolution.

### A.8 — Tests

- Phase ordering: 4 passes at mixed phases → correct `AddPasses` call order.
- `FindHandle` reverse scan: two same-named resources → returns latest.
- `PostProcessChain` chaining: two passes chain → graph compiles, topo order correct.
- `ResolvePostProcessInput`: returns `SceneColour` when no chain, `PostProcessChain` when present.
- All existing tests updated and passing.

---

## Phase B: Effect Schema System (TOML Metadata)

### B.1 — Schema file format

**New directory:** `engine/wayfinder/schemas/effects/`

```toml
# schemas/effects/colour_grading.toml
[effect]
name = "colour_grading"
display_name = "Colour Grading"
category = "rendering.post_process"

[[fields]]
name = "exposure_stops"
type = "float"
default = 0.0
min = -10.0
max = 10.0
display = "Exposure (Stops)"

# ...
```

Categories use dotted paths for organisation: `rendering.post_process`, `rendering.environment`, `camera`, `audio`, `physics`, `gameplay`.

### B.2 — Schema loader

**New files:** `engine/wayfinder/src/volumes/EffectSchema.h`, `engine/wayfinder/src/volumes/EffectSchema.cpp`

Lives in the volumes domain (not rendering) since schemas apply to all effect types.

### B.3 — Schema-driven validation

Validate effect JSON against schemas at scene load: unknown fields → warning, out-of-range → clamp + warning, wrong type → error + skip.

### B.4 — Struct↔schema sync tests

For each engine effect type: load schema, verify field count/names/types match C++ struct.

### B.5 — CMake + docs

Add schema files, source files, test files. Document schema format.

---

## Phase C: Data-Driven Pipeline Configuration

### C.1 — Pipeline config format

```toml
# config/pipelines/default.toml
[pipeline]
name = "default"

[[passes]]
name = "Bloom"
phase = "post_process"
order = 0
enabled = true

[[passes]]
name = "FXAA"
phase = "composite"
order = 0
enabled = true
```

### C.2 — Pass factory registry

Maps `RenderPass::GetName()` strings to factory functions. Engine and game register factories at startup.

### C.3 — Config loader + applier

`LoadPipelineConfig(path)` parses TOML. `ApplyPipelineConfig(renderer, factory, config)` creates/registers/enables passes. Engine-required passes auto-added.

### C.4 — Hot-reload

File watcher → reload config → diff → add/remove/update passes → next frame uses new config.

### C.5 — Path to dependency-driven injection

With schemas and configs in place, config entries can gain dependency fields:

```toml
[[passes]]
name = "DepthOfField"
phase = "post_process"
reads = ["SceneColour", "SceneDepth"]
after = ["Bloom"]
```

Config loader builds dependency graph, topologically sorts, eliminates manual `order` values. Full data-driven intent declaration without C++ virtual methods or reflection.

---

## Decisions

- **Generalised volume system.** The blendable effect system is engine-wide infrastructure, not rendering-specific. Renamed and relocated to `volumes/` domain.
- **Rendering effects stay in rendering.** `ColourGradingParams`, `VignetteParams`, `ChromaticAberrationParams` live in `rendering/materials/RenderingEffects.h` — they're rendering domain types that happen to be registered with the generic volume system.
- **Single unified pass list.** No engine/game split. `RenderPhase` + `order` for all.
- **8 render phases.** PreOpaque → Opaque → PostOpaque → Transparent → PostProcess → Composite → Overlay → Present. Covers all known use cases without over-granularity.
- **PostProcessChain is a naming convention.** Post-process passes read/write a transient named `PostProcessChain`. It's documented, not enforced. Passes outside this convention still work.
- **FindHandle returns latest.** Reverse-scan, one-line change, backward-compatible, makes chaining work.
- **Breaking Renderer API.** `AddPass(phase, order, pass)` — clean, explicit.
- **Schemas are metadata.** TOML describes effects for tools. C++ struct is runtime truth. Tests sync them.
- **Pipeline configs are optional.** C++ registration always works. Data-driven is additive.
- **Phase ordering is sequential for now.** Phases A-B are immediately implementable. Phase C builds on them.

---

## Relevant Files

### Phase 0 — Renamed/Moved
- `rendering/materials/PostProcessRegistry.h/.cpp/.inl` → `volumes/VolumeEffectRegistry.h/.cpp/.inl`
- `rendering/materials/PostProcessVolume.h/.cpp` → split into `volumes/VolumeEffect.h/.cpp` + `rendering/materials/RenderingEffects.h/.cpp`
- `rendering/materials/Override.h` → `volumes/Override.h`

### Phase 0 — Updated (include path + type name)
- `rendering/graph/RenderFrame.h`
- `rendering/pipeline/RenderContext.h/.cpp`
- `rendering/pipeline/SceneRenderExtractor.cpp`
- `rendering/pipeline/CompositionUBOUtils.h`
- `rendering/passes/CompositionPass.cpp`
- `scene/ComponentRegistry.cpp`
- `scene/Components.h`
- `tests/rendering/PostProcessRegistryTests.cpp` → `tests/core/VolumeEffectRegistryTests.cpp`
- `tests/rendering/RenderGraphTests.cpp`
- `engine/wayfinder/CMakeLists.txt`
- `docs/render_passes.md`

### Phase A — Modified
- `rendering/pipeline/RenderPipeline.h/.cpp` — `RenderPhase`, unified pass list, `BuildGraph` single-loop
- `rendering/pipeline/Renderer.h/.cpp` — new `AddPass(phase, order, pass)`, remove `m_passes`
- `rendering/graph/RenderGraph.h` — `GraphTextureId::PostProcessChain`
- `rendering/graph/RenderGraph.cpp` — `FindHandle` reverse scan
- `rendering/passes/CompositionPass.cpp` — `ResolvePostProcessInput`
- `rendering/passes/PresentSourceCopyPass.cpp` — chain convention
- `tests/rendering/RenderPassTests.cpp`, `RenderPipelineTests.cpp`, `RenderGraphTests.cpp`, `SceneOpaquePassTests.cpp`

### Phase A — New
- `rendering/graph/PostProcessChainUtils.h` (or inlined)

### Phase B — New
- `schemas/effects/colour_grading.toml`, `vignette.toml`, `chromatic_aberration.toml`
- `volumes/EffectSchema.h/.cpp`
- `tests/core/EffectSchemaTests.cpp`

### Phase C — New
- `config/pipelines/default.toml`
- `rendering/pipeline/RenderPassFactory.h/.cpp`
- `rendering/pipeline/PipelineConfig.h/.cpp`

---

## Verification

### Phase 0
1. All existing tests pass with renamed types (mechanical find-replace).
2. All includes resolve to new locations.
3. Build clean: `cmake --build --preset debug`.
4. `tools/lint.py` + `tools/tidy.py` clean.

### Phase A
1. Phase-ordering test: 4 passes at mixed phases → correct injection order.
2. FindHandle reverse-scan test: two same-named resources → returns latest.
3. PostProcessChain chain test: two passes → graph compiles, correct topo order.
4. ResolvePostProcessInput test: SceneColour fallback vs chain.
5. Journey sandbox: visual parity.

### Phase B
1. Schema loader loads all engine schemas successfully.
2. Struct↔schema sync tests pass.
3. Validation rejects bad data with correct warnings.

### Phase C
1. Config loads correctly.
2. Unknown passes → warning, skip.
3. Engine passes auto-added.
4. Hot-reload diffs applied.

---

## Critical Implementation Detail: FindHandle Must Return Latest

`RenderGraph::FindHandle` currently does a forward scan and returns the first match. `CreateTransient` always appends. Fix: reverse scan so `FindHandle` returns the latest resource with a matching name. One-line change, backward-compatible, essential for the PostProcessChain convention.

---

## Further Considerations

1. **Pass removal at runtime.** Supported; contract is no removal during `BuildGraph`. Document.
2. **Thread safety.** Single-threaded (main thread only). Document. Lock/staging buffer if MT needed — YAGNI.
3. **Non-rendering consumers.** The volume system (`VolumeEffectRegistry`) is ready for audio, camera, physics, gameplay from day one. Those systems query `VolumeEffectStack` by their registered effect IDs. No rendering dependency.
4. **Buffer resources in the render graph.** Compute passes that produce structured buffers need buffer dependency tracking in the graph. Not blocking — compute passes work today with manual buffer management — but needed for full auto-sort/cull. Separate issue.
5. **Temporal resources.** TAA/motion blur need previous frame data. Use `ImportTexture` for persistent textures (already supported). `MarkPersistent` would be a convenience extension.
