---
name: ""
overview: ""
todos: []
isProject: false
---

# Plan: Issue #143 — Automated Shader Reflection via Slang

## TL;DR

Replace all manual `ShaderResourceCounts` with Slang's `ProgramLayout` reflection API. Reflection data is extracted at compile time (runtime Slang path) and generated at build time (Shipping .spv path). `ShaderManager` becomes the single source of truth for resource counts — all consumers (`ShaderProgramDesc`, `GPUPipelineDesc`, pass code) stop carrying them.

## Architecture

**Current flow (manual):**
```text
Pass code → ShaderProgramDesc{VertexResources, FragmentResources}
  → ShaderProgramRegistry::Register() → GPUPipelineDesc{VertexResources, FragmentResources}
    → PipelineCache::GetOrCreate(ShaderManager, desc)
      → ShaderManager::GetShader(name, stage, MANUAL_COUNTS)
        → RenderDevice::CreateShader(desc with MANUAL counts)
```

**New flow (reflection-driven):**
```text
Pass code → ShaderProgramDesc (no resource counts)
  → ShaderProgramRegistry::Register() → GPUPipelineDesc (no resource counts)
    → PipelineCache::GetOrCreate(ShaderManager, desc)
      → ShaderManager::GetShader(name, stage)
        → Counts from: runtime compilation reflection OR shader_manifest.json
        → RenderDevice::CreateShader(desc with REFLECTED counts)
```

## Phases

### Phase 1: Reflection extraction in SlangCompiler

**1.1** Add `ShaderResourceCounts` to `SlangCompiler::CompileResult`.

**1.2** Implement private `ExtractResourceCounts(slang::IComponentType* linkedProgram)` in SlangCompiler.cpp.
- Call `linkedProgram->getLayout(0)` to get `ProgramLayout*`
- Get entry point at index 0 via `getEntryPointByIndex(0)`
- Query `TypeLayoutReflection` using the binding range API:
  - Iterate `getBindingRangeCount()`, check `getBindingRangeType()`:
    - `BindingType::ConstantBuffer` → UniformBuffers
    - `BindingType::CombinedTextureSampler` → Samplers
    - `BindingType::MutableTexture` / storage texture types → StorageTextures
    - `BindingType::RawBuffer` / `MutableRawBuffer` (UAV) → StorageBuffers
  - Sum `getBindingRangeBindingCount()` per category
- **Critical**: Validate the ParameterCategory vs BindingRange mapping empirically with a test before committing to one approach. If `getSize(ParameterCategory::ConstantBuffer)` gives correct counts, that's simpler than iterating binding ranges.

**1.3** Wire into `SlangCompiler::Compile()` — after the link step (line ~300 in current code), before copying SPIR-V to output, call `ExtractResourceCounts(linkedProgram.get())` and store result in `CompileResult::Resources`.

### Phase 2: ShaderManager consumes reflection instead of manual counts

**2.1** Change `ShaderManager::GetShader()` signature: remove the `const ShaderResourceCounts& resources` parameter. The method returns `GPUShaderHandle` as before — resource counts are resolved internally.

**2.2** ShaderManager gets counts from:
- **Runtime compilation path** (non-Shipping): `CompileResult::Resources` from `SlangCompiler::Compile()`
- **Pre-compiled .spv path** (Shipping): Load from `shader_manifest.json` (Phase 3)

**2.3** Store reflected counts alongside cached shader handles. Extend the cache value from `GPUShaderHandle` to a small struct:
```cpp
struct CachedShader { GPUShaderHandle Handle; ShaderResourceCounts Resources; };
```
This ensures counts are available even on cache hits.

**2.4** Use reflected counts when calling `RenderDevice::CreateShader()` — replace the manual resources parameter with the internally resolved counts.

### Phase 3: Build-time shader manifest for Shipping

**3.1** Generate `shader_manifest.json` at build time. Two options (validate which works):
- **Option A**: Add `slangc -dump-ir` / reflection output flag to build step (if slangc supports it)
- **Option B**: Build a small `shader-reflect` CMake tool target that links `Slang::slang`, compiles each shader via the API (same as SlangCompiler), extracts reflection, writes JSON. Run as a build step in `WayfinderShaders.cmake`.
- **Format**: Single JSON file listing all shaders with per-stage resource counts:
  ```json
  {
    "unlit": {
      "vertex": {"uniformBuffers": 1, "samplers": 0, "storageTextures": 0, "storageBuffers": 0},
      "fragment": {"uniformBuffers": 1, "samplers": 0, "storageTextures": 0, "storageBuffers": 0}
    },
    ...
  }
  ```

**3.2** `ShaderManager::LoadManifest(path)` — loads JSON manifest at startup. Used when SlangCompiler is unavailable (Shipping).

**3.3** Add manifest to `WayfinderShaders.cmake` output pipeline — generated alongside .spv files, synced to output directory.

### Phase 4: Remove manual ShaderResourceCounts from API surface

**4.1** Remove `VertexResources` and `FragmentResources` from `ShaderProgramDesc`.

**4.2** Remove `VertexResources` and `FragmentResources` from `GPUPipelineDesc`.

**4.3** Update `ShaderProgramRegistry::Register()` — no longer copies resource counts to GPUPipelineDesc.

**4.4** Update `PipelineCache::GetOrCreate(ShaderManager&, GPUPipelineDesc)` — calls `ShaderManager::GetShader(name, stage)` without counts.

**4.5** Update `SubmissionDrawing::MakeWireframeVariant()` — same removal.

**4.6** Update `ShaderProgramRegistry::GetVariantPipeline()` — same removal.

**4.7** Clean all pass files — remove manual resource count assignments:
- SceneOpaquePass.cpp (4 programs: unlit, unlit_blended, basic_lit, textured_lit)
- ChromaticAberrationFeature.cpp
- ColourGradingFeature.cpp
- VignetteFeature.cpp
- CompositionPass.cpp
- DebugPass.cpp (2 programs: debug_unlit, debug_unlit_solid)

### Phase 5: Tests and validation

**5.1** New test: `SlangCompiler reflection extracts correct resource counts`
- Compile `textured_lit.slang` vertex → assert Resources == {UniformBuffers=1}
- Compile `textured_lit.slang` fragment → assert Resources == {UniformBuffers=2, Samplers=1}
- Compile `fullscreen_copy.slang` vertex → assert Resources == {} (all zeros)
- Compile `fullscreen_copy.slang` fragment → assert Resources == {Samplers=1}
- This validates the ParameterCategory/BindingRange mapping is correct.

**5.2** Update `PostProcessFeatureTests.cpp` — remove assertions on `FragmentResources.UniformBuffers` and `FragmentResources.Samplers` (these fields no longer exist on ShaderProgramDesc). Keep all other assertions about Feature behaviour.

**5.3** Debug-mode validation (temporary during migration, can remove later): in `ShaderManager::GetShader()`, if the caller happens to be in a debug path, log the reflected counts so mismatches are visible in dev iteration.

**5.4** Run all existing render tests — they exercise the full pipeline with real shaders, so passing confirms reflection counts are correct end-to-end.

## Relevant Files

### Core changes
- [engine/wayfinder/src/rendering/materials/SlangCompiler.h](engine/wayfinder/src/rendering/materials/SlangCompiler.h) — extend `CompileResult` with `ShaderResourceCounts Resources;`
- [engine/wayfinder/src/rendering/materials/SlangCompiler.cpp](engine/wayfinder/src/rendering/materials/SlangCompiler.cpp) — add `ExtractResourceCounts()`, wire into `Compile()`
- [engine/wayfinder/src/rendering/materials/ShaderManager.h](engine/wayfinder/src/rendering/materials/ShaderManager.h) — remove `resources` param from `GetShader()`, add `CachedShader` struct, add `LoadManifest()`
- [engine/wayfinder/src/rendering/materials/ShaderManager.cpp](engine/wayfinder/src/rendering/materials/ShaderManager.cpp) — resolve counts internally, manifest loading, cache restructure
- [engine/wayfinder/src/rendering/materials/ShaderProgram.h](engine/wayfinder/src/rendering/materials/ShaderProgram.h) — remove `VertexResources`/`FragmentResources` from `ShaderProgramDesc`
- [engine/wayfinder/src/rendering/materials/ShaderProgram.cpp](engine/wayfinder/src/rendering/materials/ShaderProgram.cpp) — stop copying resource counts in `Register()` and `GetVariantPipeline()`
- [engine/wayfinder/src/rendering/pipeline/PipelineCache.h](engine/wayfinder/src/rendering/pipeline/PipelineCache.h) — remove `VertexResources`/`FragmentResources` from `GPUPipelineDesc`
- [engine/wayfinder/src/rendering/pipeline/PipelineCache.cpp](engine/wayfinder/src/rendering/pipeline/PipelineCache.cpp) — update `GetOrCreate()` call

### Pass files (remove manual counts)
- [engine/wayfinder/src/rendering/passes/SceneOpaquePass.cpp](engine/wayfinder/src/rendering/passes/SceneOpaquePass.cpp) — 8 lines removed (4 programs × 2 resource fields)
- [engine/wayfinder/src/rendering/passes/ChromaticAberrationFeature.cpp](engine/wayfinder/src/rendering/passes/ChromaticAberrationFeature.cpp) — 2 lines removed
- [engine/wayfinder/src/rendering/passes/ColourGradingFeature.cpp](engine/wayfinder/src/rendering/passes/ColourGradingFeature.cpp) — 2 lines removed
- [engine/wayfinder/src/rendering/passes/VignetteFeature.cpp](engine/wayfinder/src/rendering/passes/VignetteFeature.cpp) — 2 lines removed
- [engine/wayfinder/src/rendering/passes/CompositionPass.cpp](engine/wayfinder/src/rendering/passes/CompositionPass.cpp) — 2 lines removed
- [engine/wayfinder/src/rendering/passes/DebugPass.cpp](engine/wayfinder/src/rendering/passes/DebugPass.cpp) — 4 lines removed
- [engine/wayfinder/src/rendering/passes/SubmissionDrawing.cpp](engine/wayfinder/src/rendering/passes/SubmissionDrawing.cpp) — update GetShader() calls

### Build system
- [cmake/WayfinderShaders.cmake](cmake/WayfinderShaders.cmake) — add manifest generation step

### Tests
- [tests/rendering/PostProcessFeatureTests.cpp](tests/rendering/PostProcessFeatureTests.cpp) — remove resource count assertions
- New test file or section in existing rendering tests — SlangCompiler reflection tests

## Verification

1. **Reflection correctness test**: Compile known shaders, assert extracted counts match expected values (Phase 5.1)
2. **Build**: `cmake --build --preset debug` — all targets compile with no errors
3. **Unit tests**: `ctest --preset test` — all existing tests pass (after PostProcessFeatureTests update)
4. **Runtime validation**: Run `journey` in Debug — engine starts, renders correctly with reflection-derived counts
5. **Lint/tidy**: `tools/lint.py --changed` and `tools/tidy.py --changed` pass
6. **Shipping build**: `cmake --build --preset shipping --target journey` — verifies manifest loading path compiles (full runtime validation when manifest generation is wired)

## Decisions

- **Binding range API preferred over ParameterCategory**: More precise mapping to SDL_GPU's resource categories (especially for combined image samplers). Validate empirically in Phase 1.2.
- **Single manifest file over per-shader sidecars**: Cleaner, one file load at startup, data-driven.
- **ShaderManager is the single source of truth for resource counts**: No consumer needs to specify or carry them. Clean separation of concerns.
- **ShaderResourceCounts struct itself is kept**: It's a useful internal type for RenderDevice::CreateShader(). Only its presence in ShaderProgramDesc/GPUPipelineDesc is removed.
- **`LoadComputeShaderBytecode()` out of scope**: Compute shaders don't use ShaderResourceCounts in the current path — they use `CreateComputePipeline()`. Can add compute reflection separately if needed.

## Further Considerations

1. **ParameterCategory vs BindingRange mapping**: The correct Slang reflection query depends on how `Sampler2D<float4>` (combined image sampler) is categorized. The binding range API (`getBindingRangeType() == CombinedTextureSampler`) is more explicit. First implementation task should be a reflection test that validates the mapping.

2. **Build-time reflection tool**: If `slangc` doesn't have a built-in reflection JSON output flag, we need a small C++ tool linked against `Slang::slang`. This could be a new CMake target (`shader_reflect`) or a `waypoint reflect` subcommand. Recommend a standalone target for simplicity — it only needs SlangCompiler + JSON serialisation.

3. **Hot-reload integration**: When `ReloadShaders()` is called, the reflection cache must also be invalidated. Since ShaderManager already clears its cache, and re-compilation will re-extract reflection, this should work naturally. Worth a quick validation.