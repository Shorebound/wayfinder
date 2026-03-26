# Plan: Material-Owned Sampler State (Issue #120)

## TL;DR
Sampler configuration (filter, address mode) currently lives on `TextureAsset` and is read during material resolution in `RenderResources::ResolveTextureBindings()`. This prevents the same texture from being sampled differently by different materials. Move sampler authority to the material descriptor, with a layered fallback chain: material per-slot → material default → shader slot default → engine default. Extract the sampler cache from `TextureManager` into a standalone `SamplerCache`.

## Current State
- `TextureAsset` stores `Filter` and `AddressMode` (parsed from texture JSON)
- `TextureManager::GetOrCreateSampler()` has a FNV-1a hash-based sampler cache
- `RenderResources::ResolveTextureBindings()` reads the *texture's* filter/address mode to build `SamplerCreateDesc`, auto-enables trilinear+aniso for linear+mips textures
- `MaterialAsset` has no sampler configuration — just `Textures` map (slot name → AssetId)
- `TextureSlotDecl` is just `Name + BindingSlot`, no sampler info
- `TextureBindingSet` just maps slot names to AssetIds
- `RenderMaterialBinding` carries `TextureBindingSet` + resolved `ResolvedTextureBinding` (GPU handles)

## Engine Comparison

| Engine | Sampler Ownership | Details |
|--------|-------------------|---------|
| **Unreal** | Material | Each TextureSample node owns sampler type + address mode. Shared sampler slots limit (16/material). Full per-sample control. |
| **Godot 4** | Shader/Material | Shader uniforms declare `filter_nearest`, `repeat_enable` as hints. Materials override. |
| **bgfx** | Per-draw-call | `BGFX_SAMPLER_*` flags set at bind time, fully independent of texture creation. |
| **Spartan** | Separate resource | Materials specify both texture and sampler bindings explicitly. |
| **Bevy** | Image (texture) | `Image.sampler` can be Default or custom `ImageSamplerDescriptor`. NOT material-owned — but materials can override via custom pipelines. |

**Consensus**: 4 of 5 engines associate sampler state with the material/binding point. Bevy is the outlier but is also simpler (no material graph). Wayfinder should follow the majority: **material-owned samplers**.

## Design

### MaterialSamplerDesc — High-Level Sampler Configuration

```
struct MaterialSamplerDesc {
    SamplerFilter Filter = SamplerFilter::Linear;
    SamplerAddressMode AddressMode = SamplerAddressMode::Repeat;
};
```

Same two fields as current `TextureAsset` — intentionally simple. The auto-trilinear/aniso logic stays in the resolver (derived from texture mip chain at bind time). Advanced fields (mipmap mode, aniso level, LOD bias) can be added later as optional overrides.

### Resolution Chain (priority order)
1. **Material per-slot sampler** (`MaterialAsset::Samplers["diffuse"]`) — most specific
2. **Material default sampler** (`MaterialAsset::DefaultSampler`) — material-wide default
3. **Shader slot default** (`TextureSlotDecl::DefaultSampler`) — shader-declared default
4. **Engine default** (Linear + Repeat + auto trilinear/aniso when mips) — implicit fallback

### Material JSON Format
```json
{
  "asset_type": "material",
  "shader": "textured_lit",
  "textures": { "diffuse": "texture-asset-id" },
  "default_sampler": { "filter": "linear", "address_mode": "repeat" },
  "samplers": {
    "diffuse": { "filter": "nearest", "address_mode": "clamp" }
  }
}
```
- `default_sampler`: optional, material-level default
- `samplers`: optional, per-slot overrides (keys match texture slot names)
- Both are optional — existing materials work unchanged via engine defaults

### SamplerCache — Extracted from TextureManager
Standalone class owning the `unordered_map<uint64_t, GPUSamplerHandle>` and FNV-1a hash. Owned by `RenderContext` alongside `TextureManager`, `PipelineCache`, etc. Fulfils issue requirement: "TextureManager no longer creates or caches samplers."

---

## Steps

### Phase 1: Data Model (no behaviour change yet)

**Step 1.** Add `MaterialSamplerDesc` to `RenderTypes.h`
- Two fields: `Filter`, `AddressMode`
- Helper method: `ToCreateDesc(bool hasMips) → SamplerCreateDesc` (encapsulates the auto-trilinear/aniso logic currently inline in `ResolveTextureBindings`)

**Step 2.** Add `DefaultSampler` to `TextureSlotDecl` in `ShaderProgram.h`
- `std::optional<MaterialSamplerDesc> DefaultSampler`
- No changes to existing shader program registrations (they leave it nullopt)

**Step 3.** Add sampler fields to `MaterialAsset` in `Material.h`
- `std::unordered_map<std::string, MaterialSamplerDesc> Samplers` — per-slot
- `std::optional<MaterialSamplerDesc> DefaultSampler` — material-level

**Step 4.** Add sampler data to `TextureBindingSet` in `RenderFrame.h`
- `std::unordered_map<std::string, MaterialSamplerDesc> Samplers`
- `std::optional<MaterialSamplerDesc> DefaultSampler`
- Carries sampler config through the frame alongside texture slot bindings

### Phase 2: Sampler Cache Extraction

**Step 5.** Create `SamplerCache` class
- New files: `engine/wayfinder/src/rendering/resources/SamplerCache.h/.cpp`
- Move `GetOrCreateSampler()`, `HashSamplerDesc()`, and `m_samplerCache` from `TextureManager`
- Same FNV-1a hash, same `unordered_map<uint64_t, GPUSamplerHandle>` cache
- `Initialise(RenderDevice&)`, `Shutdown()`, `GetOrCreate(const SamplerCreateDesc&) → GPUSamplerHandle`

**Step 6.** Wire `SamplerCache` into `RenderContext`
- `RenderContext` owns `std::unique_ptr<SamplerCache>`
- Initialised/shutdown alongside other shared resources
- Expose via `GetSamplerCache() → SamplerCache&`

**Step 7.** Remove sampler caching from `TextureManager`
- Delete `GetOrCreateSampler()`, `HashSamplerDesc()`, `m_samplerCache`
- TextureManager becomes purely texture-focused (fulfils Definition of Done)

**Step 8.** Wire `SamplerCache` into `RenderResourceCache`
- Add `SetSamplerCache(SamplerCache*)` method
- Update `Renderer` initialisation to pass the sampler cache through

### Phase 3: Resolution Logic

**Step 9.** Update `Material.cpp` — Parse samplers from JSON
- Parse `"default_sampler"` object → `MaterialSamplerDesc`
- Parse `"samplers"` object → per-slot `MaterialSamplerDesc` map
- Validate slot names against `textures` keys (warn on mismatch)
- `ParseSamplerDesc()` helper: reads `filter` (nearest/linear) and `address_mode` (repeat/clamp/mirrored_repeat)

**Step 10.** Update `CreateMaterialResource()` in `RenderResources.cpp`
- Copy `MaterialAsset::Samplers` → `TextureBindingSet::Samplers`
- Copy `MaterialAsset::DefaultSampler` → `TextureBindingSet::DefaultSampler`

**Step 11.** Rewrite `ResolveTextureBindings()` in `RenderResources.cpp`
- For each texture slot:
  1. Look up per-slot sampler from `binding.Textures.Samplers[slotName]`
  2. Fallback to `binding.Textures.DefaultSampler`
  3. Fallback to `slotDecl.DefaultSampler`
  4. Fallback to engine default (`MaterialSamplerDesc{}` = Linear + Repeat)
  5. Call `MaterialSamplerDesc::ToCreateDesc(hasMips)` to build `SamplerCreateDesc`
  6. Call `m_samplerCache->GetOrCreate(samplerDesc)` instead of `m_textureManager->GetOrCreateSampler()`
- Remove the `TextureAsset*` fetch that was only used for sampler info (texture GPU handle still loaded via `GetOrLoad`)
- Note: `hasMips` still comes from the loaded texture metadata (needed for auto-trilinear/aniso)

**Step 12.** Update `RenderPipeline.cpp` — composition pass sampler
- The nearest sampler for the composition blit currently comes from `RenderContext::GetNearestSampler()`
- This is NOT a material-owned sampler — it's a system sampler. No change needed, but verify it still works through `SamplerCache`.

### Phase 4: Content Migration

**Step 13.** Update sandbox material JSON files
- `textured_crate_material.json` → add `"samplers": { "diffuse": { "filter": "nearest", "address_mode": "repeat" } }` (was inherited from test_checker texture)
- Other materials using linear+repeat need no changes (matches engine default)
- Survey all materials in `sandbox/journey/assets/materials/` for non-default sampling needs

**Step 14.** Document texture asset fields as hints
- Update `TextureAsset.h` doc comments: `Filter` and `AddressMode` are asset preview hints, not authoritative for rendering
- Texture JSON `filter` and `address_mode` fields remain valid for tooling

### Phase 5: Tests

**Step 15.** Add `MaterialSamplerDesc` unit tests — *parallel with step 16*
- `ToCreateDesc()` produces correct `SamplerCreateDesc` for linear/nearest, with/without mips
- Auto-trilinear and aniso logic when mips + linear

**Step 16.** Add material JSON sampler parsing tests — *parallel with step 15*
- Parse `default_sampler` block
- Parse per-slot `samplers` block
- Missing sampler = no entry (not a default-constructed one)
- Invalid filter/address_mode values → error or default
- Sampler slot name not matching any texture → warning

**Step 17.** Add sampler resolution chain tests — *depends on steps 5, 9, 11*
- Per-slot sampler overrides engine default
- Material default overrides engine default
- Per-slot overrides material default
- Shader slot default used when material has no sampler
- Same texture, two materials with different samplers → different GPU sampler handles
- No sampler specified anywhere → engine default (Linear + Repeat)

**Step 18.** Update `SamplerCache` tests (moved from TextureManager tests)
- Deduplication (same desc → same handle)
- Differentiation (different desc → different handle)
- Mipmap mode differentiation
- Anisotropy differentiation
- Move relevant tests from `TextureManagerTests.cpp` to new `SamplerCacheTests.cpp`

**Step 19.** Update `TextureManagerTests.cpp`
- Remove sampler-related tests (moved to SamplerCacheTests)
- Verify `TextureManager` no longer exposes sampler API

### Phase 6: Verification

**Step 20.** Build all configurations
- `cmake --build --preset debug` (full build)
- `ctest --preset test` (all tests pass)

**Step 21.** Manual verification in Journey sandbox
- Textured crate renders with nearest filtering (from material, not texture)
- Brick wall renders with linear filtering (engine default)
- Same texture bound with different samplers in different materials works correctly

---

## Relevant Files

### Modify
- `engine/wayfinder/src/rendering/RenderTypes.h` — add `MaterialSamplerDesc`
- `engine/wayfinder/src/rendering/materials/Material.h` — add `Samplers`, `DefaultSampler` to `MaterialAsset`
- `engine/wayfinder/src/rendering/materials/Material.cpp` — parse samplers from JSON
- `engine/wayfinder/src/rendering/materials/ShaderProgram.h` — add `DefaultSampler` to `TextureSlotDecl`
- `engine/wayfinder/src/rendering/graph/RenderFrame.h` — add samplers to `TextureBindingSet`
- `engine/wayfinder/src/rendering/resources/RenderResources.h` — add `SetSamplerCache()`
- `engine/wayfinder/src/rendering/resources/RenderResources.cpp` — rewrite `ResolveTextureBindings()`
- `engine/wayfinder/src/rendering/resources/TextureManager.h` — remove `GetOrCreateSampler()`, `m_samplerCache`
- `engine/wayfinder/src/rendering/resources/TextureManager.cpp` — remove sampler caching code
- `engine/wayfinder/src/rendering/pipeline/RenderContext.h` — own `SamplerCache`
- `engine/wayfinder/src/rendering/pipeline/RenderContext.cpp` — init/shutdown `SamplerCache`
- `engine/wayfinder/src/rendering/pipeline/Renderer.cpp` — wire `SamplerCache` into `RenderResourceCache`
- `sandbox/journey/assets/materials/textured_crate_material.json` — add sampler block
- `tests/rendering/TextureManagerTests.cpp` — remove sampler tests

### Create
- `engine/wayfinder/src/rendering/resources/SamplerCache.h` — new sampler cache class
- `engine/wayfinder/src/rendering/resources/SamplerCache.cpp` — implementation
- `tests/rendering/SamplerCacheTests.cpp` — sampler cache tests (moved + new)
- `tests/fixtures/material_with_samplers.json` — test fixture
- `tests/fixtures/material_default_sampler.json` — test fixture

### Reference Only
- `engine/wayfinder/src/assets/TextureAsset.h` — doc update only (fields become hints)
- `engine/wayfinder/src/rendering/backend/sdl_gpu/SDLGPUDevice.cpp` — no changes needed (CreateSampler stays)
- `engine/wayfinder/src/rendering/backend/null/NullDevice.h` — no changes needed

## Verification
1. All tests pass: `ctest --preset test`
2. Build clean in all configs: `cmake --build --preset debug`, `cmake --build --preset development`
3. Same texture renders with different samplers in two different materials (verified in Journey)
4. Existing materials with no sampler blocks render identically to before (Linear + Repeat default)
5. Textured crate material now explicitly specifies nearest filtering (was implicit from texture)

## Decisions
- **MaterialSamplerDesc is intentionally minimal** (Filter + AddressMode). Advanced fields (mipmap mode, LOD bias, explicit anisotropy) are future additions as optional overrides.
- **Texture JSON keeps filter/address_mode** as tooling/preview hints. Not authoritative at runtime.
- **SamplerCache extracted from TextureManager** per issue requirement ("TextureManager no longer creates or caches samplers").
- **Engine default is Linear + Repeat** with auto-trilinear+aniso@4x for linear+mips textures. Matches current behaviour for the common case.
- **`TextureAsset` filter/address_mode fields not consulted during rendering** post-migration. Clean break from texture-driven samplers.
- **Composition blit sampler** (nearest, clamp) owned by `RenderContext` is NOT material-owned — it's a system/pipeline sampler. No change.

## Further Considerations
1. **Shader slot defaults**: Currently planned but no shader programs will populate them initially. Useful when we add specialised shaders (e.g., a normal map shader that defaults to linear+clamp). Could be deferred if it complicates the initial implementation. **Recommendation**: Include the field now (it's one `optional`), populate later.
2. **hasMips resolution**: Auto-trilinear needs to know if the texture has mips. Currently derived from `TextureAsset::MipLevels`. Post-migration, we still load the `TextureAsset` for mip info but NOT for sampler info. We could cache mip-count on the GPU texture handle instead. **Recommendation**: Keep reading `TextureAsset::MipLevels` for now; optimise later when texture metadata is stored on GPU handles.
3. **Content migration scope**: Only `textured_crate_material.json` needs explicit nearest sampling. All others use Linear+Repeat (engine default). Minimal migration effort.
