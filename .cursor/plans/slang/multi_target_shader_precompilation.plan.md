# Plan: Multi-Target Shader Precompilation (#58)

## TL;DR

Extend the shader build pipeline to produce per-platform bytecode bundles (SPIR-V, DXIL, MSL, WGSL) from `.slang` sources via a configurable CMake target list. At runtime, `ShaderManager` loads the native bytecode for the active GPU backend, eliminating SDL_GPU's runtime SPIR-V cross-compilation overhead. WGSL is build-only until a WebGPU backend exists. Development builds can run SPIR-V-only (SDL_GPU cross-compiles) with runtime Slang fallback; Shipping builds require pre-compiled native bytecode.

**Approach:** Engine-side `ShaderFormat` enum + device format query → CMake multi-target compilation loop → `ShaderManager` format-aware load with fallback chain → `SDLGPUDevice` multi-format `CreateShader`.

---

## Decisions

- **Directory layout:** Subdirectories per format - `shaders/spirv/`, `shaders/dxil/`, `shaders/metal/`, `shaders/wgsl/`. Breaking change from current flat layout; all paths resolve through `ShaderManager` so migration is localised.
- **Targets in scope:** SPIR-V (Vulkan), DXIL (D3D12), MSL (Metal), WGSL (WebGPU - build-only, deferred runtime loading).
- **Build scope:** Cache variable `WAYFINDER_SHADER_TARGETS` with platform-aware defaults. Presets override per-config. Dev preset defaults to `spirv`; shipping preset can list all four.
- **WGSL:** slangc produces valid WGSL at build time. No `SDL_GPU_SHADERFORMAT_WGSL` exists, so runtime loading is deferred until a WebGPU backend is added. The `ShaderFormat` enum includes it; `ShaderManager` skips loading formats the device doesn't support.
- **Manifest:** `shader_manifest.json` from #143 is format-agnostic (describes binding counts, not bytecode). Shared across all targets, placed at `shaders/shader_manifest.json`.
- **Runtime SlangCompiler:** Stays SPIR-V-only for Development/Debug fallback. SPIR-V works on all SDL_GPU backends via cross-compilation. No multi-target runtime compilation needed.
- **Waypoint:** Does not handle GPU shaders (mesh import/validation only) -- no changes needed.
- **Assumes #143 is done:** shader_manifest.json, reflection-driven resource counts, ShaderManager internal count resolution all in place.

---

## Phase 1: Shader Format Infrastructure

Engine-side types and device query interface. No behaviour changes yet.

### Step 1.1: Add `ShaderFormat` enum and utilities

**File:** `engine/wayfinder/src/rendering/backend/RenderDevice.h`

Add near the top alongside `ShaderStage`:

```
enum class ShaderFormat : uint8_t { SPIRV, DXIL, MSL, WGSL, Unknown };
```

Add a free function (or in a small `ShaderFormat.h` if preferred):
- `ShaderFormatToExtension(ShaderFormat)` -> `".spv"`, `".dxil"`, `".metal"`, `".wgsl"`
- `ShaderFormatToSubdirectory(ShaderFormat)` -> `"spirv"`, `"dxil"`, `"metal"`, `"wgsl"`
- `ShaderFormatDisplayName(ShaderFormat)` -> `"SPIR-V"`, `"DXIL"`, `"MSL"`, `"WGSL"` (for logging)

These are `constexpr` switch functions. Use `std::unreachable()` for the `Unknown` case.

### Step 1.2: Add `Format` field to `ShaderCreateDesc`

**File:** `engine/wayfinder/src/rendering/backend/RenderDevice.h`

Add to `ShaderCreateDesc`:
```
ShaderFormat Format = ShaderFormat::SPIRV;
```

This tells `RenderDevice::CreateShader` what bytecode format `Code` contains.

### Step 1.3: Add format query virtual methods to `RenderDevice`

**File:** `engine/wayfinder/src/rendering/backend/RenderDevice.h`

Add two pure virtual methods:
- `virtual ShaderFormat GetPreferredShaderFormat() const = 0` -- the native format for this device (used by ShaderManager to pick the best bytecode to load)
- `virtual bool SupportsShaderFormat(ShaderFormat format) const = 0` -- whether this device can consume the given format

### Step 1.4: Implement in SDLGPUDevice

**File:** `engine/wayfinder/src/rendering/backend/sdl_gpu/SDLGPUDevice.h/.cpp`

`GetPreferredShaderFormat()`: Read `m_shaderFormats` bitmask (from `SDL_GetGPUShaderFormats()`), return preference order:
1. `SDL_GPU_SHADERFORMAT_DXIL` present -> `ShaderFormat::DXIL`
2. `SDL_GPU_SHADERFORMAT_MSL` present -> `ShaderFormat::MSL`
3. `SDL_GPU_SHADERFORMAT_SPIRV` present -> `ShaderFormat::SPIRV`
4. else `ShaderFormat::Unknown`

Rationale: DXIL/MSL are native formats (zero translation overhead). SPIR-V requires cross-compilation on D3D12/Metal.

`SupportsShaderFormat()`: Map `ShaderFormat` -> `SDL_GPUShaderFormat` flag, check `m_shaderFormats & flag`.

### Step 1.5: Implement in NullDevice

**File:** `engine/wayfinder/src/rendering/backend/null/NullDevice.h/.cpp`

- `GetPreferredShaderFormat()` -> `ShaderFormat::SPIRV`
- `SupportsShaderFormat()` -> returns true for SPIRV (and optionally all formats, since NullDevice is a no-op)

### Step 1.6: Update SDLGPUDevice::CreateShader for multi-format

**File:** `engine/wayfinder/src/rendering/backend/sdl_gpu/SDLGPUDevice.cpp`

Replace the hardcoded SPIR-V assumption:

Current:
```cpp
info.format = static_cast<SDL_GPUShaderFormat>(m_shaderFormats & SDL_GPU_SHADERFORMAT_SPIRV);
```

New: Map `desc.Format` to the appropriate `SDL_GPUShaderFormat` flag. Validate the device supports it via `m_shaderFormats`. Clear error message if format unsupported.

For MSL: SDL_GPU accepts MSL as source text via `SDL_GPU_SHADERFORMAT_MSL`. The code/codeSize fields contain the source string.

---

## Phase 2: Multi-Format Device Creation

### Step 2.1: Broaden SDL_GPU format request

**File:** `engine/wayfinder/src/rendering/backend/sdl_gpu/SDLGPUDevice.cpp` (`Initialise()`)

Current:
```cpp
m_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
```

Change to request the platform-relevant superset:
```
SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL
```

SDL_GPU selects the best driver that supports at least one requested format. On Linux with Vulkan-only, DXIL/MSL flags are harmlessly ignored. On Windows, SDL_GPU may prefer D3D12 (native DXIL) if available. On macOS, Metal (native MSL).

This is a single-line change. The rest of the code already queries `SDL_GetGPUShaderFormats()` post-creation.

### Step 2.2: Log chosen format

After device creation, log `GetPreferredShaderFormat()` so developers know which format the backend expects:
```
SDLGPUDevice: Preferred shader format: DXIL (D3D12 backend)
```

---

## Phase 3: CMake Multi-Target Compilation

The core build-time change. Module precompilation stays shared (`.slang-module` IR is target-independent).

### Step 3.1: Add `WAYFINDER_SHADER_TARGETS` cache variable

**File:** `CMakeLists.txt` (root, near other options)

```cmake
# Shader bytecode targets. Semicolon-separated list.
# Presets override per-config. Valid values: spirv, dxil, metal, wgsl
if(WIN32)
    set(_DEFAULT_SHADER_TARGETS "spirv;dxil")
elseif(APPLE)
    set(_DEFAULT_SHADER_TARGETS "spirv;metal")
else()
    set(_DEFAULT_SHADER_TARGETS "spirv")
endif()
set(WAYFINDER_SHADER_TARGETS "${_DEFAULT_SHADER_TARGETS}" CACHE STRING
    "Shader compilation targets (spirv, dxil, metal, wgsl)")
message(STATUS "Shader targets: ${WAYFINDER_SHADER_TARGETS}")
```

### Step 3.2: Update CMakePresets.json

**File:** `CMakePresets.json`

Add `WAYFINDER_SHADER_TARGETS` overrides to relevant presets:
- `dev`: leave at default (or explicitly `"spirv"` for speed)
- `shipping`: `"spirv;dxil;metal;wgsl"` (full bundle for cross-platform)
- `ci-linux`: `"spirv"` (minimal)

### Step 3.3: Extend `wayfinder_compile_shaders()`

**File:** `cmake/WayfinderShaders.cmake`

Add optional `TARGETS` parameter to the function signature. Default to `WAYFINDER_SHADER_TARGETS`.

**New compilation loop structure:**

For the TARGETS parameter, define per-target slangc flags as a CMake map:

| Target   | slangc flags                                              | Output ext |
|----------|----------------------------------------------------------|------------|
| `spirv`  | `-target spirv -emit-spirv-directly -fvk-use-entrypoint-name` | `.spv`     |
| `dxil`   | `-target dxil -profile sm_6_0`                            | `.dxil`    |
| `metal`  | `-target metal`                                           | `.metal`   |
| `wgsl`   | `-target wgsl`                                            | `.wgsl`    |

Outer loop: `foreach(TARGET_FMT IN LISTS SHADER_TARGETS)`. Inner loop: `foreach(PROGRAM IN LISTS ARG_PROGRAMS)`.

Each target gets its own staging subdirectory: `STAGING_DIR/${TARGET_FMT}/`.

Per-program, two slangc invocations (vertex + fragment), outputting to `STAGING_DIR/${TARGET_FMT}/${stem}.vert.${ext}` and `${stem}.frag.${ext}`.

Module precompilation (`.slang-module` IR) is target-independent -- happens once, all targets share via `-I MODULE_CACHE_DIR`.

The sync step copies each target subdirectory to `OUTPUT_DIR/${TARGET_FMT}/`.

The `shader_manifest.json` from #143 is still a single file, placed at `OUTPUT_DIR/shader_manifest.json` (shared).

### Step 3.4: Stamp file and dependency structure

Each target's outputs have a separate custom command. Add a per-target stamp:

```
${ARG_TARGET}_shaders_spirv
${ARG_TARGET}_shaders_dxil
${ARG_TARGET}_shaders_metal
${ARG_TARGET}_shaders_wgsl
```

Roll up into `${ARG_TARGET}_shaders` (ALL target). The main target depends on this.

---

## Phase 4: ShaderManager Format-Aware Loading

### Step 4.1: Store preferred format

**File:** `engine/wayfinder/src/rendering/materials/ShaderManager.h/.cpp`

Add `m_preferredFormat` member (`ShaderFormat`). Set during `Initialise()`:

```cpp
void ShaderManager::Initialise(RenderDevice& device, std::string_view shaderDirectory, SlangCompiler* compiler)
{
    m_device = &device;
    m_compiler = compiler;
    m_shaderDir = Platform::ResolvePathFromBase(shaderDirectory);
    m_preferredFormat = device.GetPreferredShaderFormat();
    Log::Info(LogRenderer, "ShaderManager: preferred format: {}", ShaderFormatDisplayName(m_preferredFormat));
}
```

### Step 4.2: Format-aware path resolution

**Current path:** `{m_shaderDir}/{name}.{stage}.spv`

**New path:** `{m_shaderDir}/{formatSubdir}/{name}.{stage}.{ext}`

Add private helper:
```cpp
std::string ShaderManager::ResolveBytecodeFilePath(
    std::string_view name, ShaderStage stage, ShaderFormat format) const
```
Builds: `m_shaderDir / ShaderFormatToSubdirectory(format) / (name + stageSuffix + ShaderFormatToExtension(format))`

### Step 4.3: Fallback chain in GetShader

**File:** `engine/wayfinder/src/rendering/materials/ShaderManager.cpp`

Replace the current single-path load with a fallback chain:

1. **Native format:** Try `ResolveBytecodeFilePath(name, stage, m_preferredFormat)`
2. **SPIR-V fallback:** If native != SPIRV and native file not found, try `ResolveBytecodeFilePath(name, stage, ShaderFormat::SPIRV)`
3. **Runtime compilation:** If both absent and `!WAYFINDER_SHIPPING`, try `m_compiler->Compile()` (produces SPIR-V)
4. **Hard error:** If all three fail, log error and return invalid handle

Track which format was actually loaded, set `ShaderCreateDesc::Format` accordingly.

### Step 4.4: LoadComputeShaderBytecode

Apply the same fallback logic to `LoadComputeShaderBytecode()`. Return a struct that includes the format alongside the bytecode (or store it alongside).

---

## Phase 5: Configuration & Wiring

### Step 5.1: RenderServices passes format context

**File:** `engine/wayfinder/src/rendering/pipeline/RenderServices.cpp`

`ShaderManager::Initialise()` already receives `RenderDevice&`. Step 4.1 adds `GetPreferredShaderFormat()` call internally. No change needed to `RenderServices` -- the device reference is sufficient.

### Step 5.2: Verify #143 manifest generation is format-agnostic

**Check:** Ensure the `wayfinder_shader_manifest` build tool doesn't depend on SPIR-V bytecode for reflection. If it currently does `SpirvResourceCounts::Count()` on the output, consider switching to `SlangReflection::ExtractResourceCounts(linkedProgram)` which operates on Slang IR (pre-codegen, format-agnostic). This is a minor change in the manifest tool -- not blocking but cleaner.

If #143 already uses the Slang reflection API path, no change needed.

### Step 5.3: EngineConfig - optional format override

**File:** `engine/wayfinder/src/app/EngineConfig.h`

Consider adding an optional format override to `ShaderConfig`:
```cpp
struct ShaderConfig
{
    std::string Directory = "assets/shaders";
    std::string SourceDirectory;
    std::optional<ShaderFormat> FormatOverride;  // Force specific format (testing/debugging)
};
```

If set, `ShaderManager` uses this instead of `device.GetPreferredShaderFormat()`. Useful for testing DXIL loading on a Vulkan device (it would fail at CreateShader, validating the error path).

This is low-priority and can be deferred.

---

## Phase 6: Testing

### Step 6.1: Format utility tests

**File:** `tests/rendering/ShaderFormatTests.cpp` (new)

- `ShaderFormatToExtension` returns correct extension for each format
- `ShaderFormatToSubdirectory` returns correct subdirectory name
- Round-trip: format -> subdirectory -> back (if applicable)

### Step 6.2: ShaderManager format resolution tests

**File:** `tests/rendering/ShaderManagerTests.cpp` (new or extend existing)

Using NullDevice and test fixture shaders:
- Load shader with preferred format present -> loads from preferred subdirectory
- Load shader with preferred format absent, SPIR-V present -> falls back to SPIR-V
- Load shader with both absent, runtime compiler available -> compiles from source
- Load shader with all absent -> returns invalid handle
- Verify `ShaderCreateDesc::Format` matches the loaded format

**Fixtures:** Create minimal test fixture directories:
```
tests/fixtures/shaders/spirv/test_shader.vert.spv
tests/fixtures/shaders/dxil/test_shader.vert.dxil
```
(Can be dummy bytecode for NullDevice -- it doesn't validate content.)

### Step 6.3: SDLGPUDevice format mapping tests

These would require a real device, so they're integration tests. Document as manual verification steps:
- On Windows: verify device reports DXIL preferred when D3D12 is active
- On macOS: verify device reports MSL preferred when Metal is active
- On Linux: verify device reports SPIRV preferred

### Step 6.4: CMake output structure validation

Add a CTest that checks the build output directory structure after compilation:
- Verify `shaders/spirv/` exists and contains expected .spv files
- Verify `shaders/dxil/` exists (on Windows preset) and contains .dxil files
- Verify `shader_manifest.json` exists at `shaders/` root

This can be a simple Python script or CMake test that checks file existence.

---

## Relevant Files

**Modify:**
- `engine/wayfinder/src/rendering/backend/RenderDevice.h` -- `ShaderFormat` enum, `ShaderCreateDesc::Format`, virtual format query methods
- `engine/wayfinder/src/rendering/backend/sdl_gpu/SDLGPUDevice.h/.cpp` -- multi-format device creation, format query implementations, format-aware `CreateShader`
- `engine/wayfinder/src/rendering/backend/null/NullDevice.h/.cpp` -- format query stubs
- `engine/wayfinder/src/rendering/materials/ShaderManager.h/.cpp` -- `m_preferredFormat`, format-aware path resolution, fallback chain
- `cmake/WayfinderShaders.cmake` -- multi-target compilation loop, per-format staging/output directories
- `CMakeLists.txt` (root) -- `WAYFINDER_SHADER_TARGETS` cache variable
- `CMakePresets.json` -- target defaults per preset
- `sandbox/journey/CMakeLists.txt` -- pass targets to `wayfinder_compile_shaders()` (or inherit from cache var)

**Create:**
- `tests/rendering/ShaderFormatTests.cpp` -- format utility and ShaderManager format resolution tests
- Test fixtures: `tests/fixtures/shaders/spirv/`, `tests/fixtures/shaders/dxil/` with dummy bytecode

**Reference (from #143, verify consistency):**
- `engine/wayfinder/src/rendering/materials/SlangReflection.h/.cpp` -- confirm format-agnostic reflection
- `tools/shader_manifest/` -- confirm manifest generation doesn't depend on SPIR-V bytecode

**No changes needed:**
- `tools/waypoint/` -- does not handle GPU shaders
- `engine/wayfinder/src/rendering/materials/SlangCompiler.h/.cpp` -- runtime compiler stays SPIR-V-only
- `engine/wayfinder/src/rendering/pipeline/RenderServices.cpp` -- device ref already passed through

---

## Verification

1. **Build test (Windows, dev preset):** `cmake --preset dev && cmake --build --preset debug` -- verify `bin/Debug/assets/shaders/spirv/*.spv` and `shaders/dxil/*.dxil` are produced
2. **Build test (Linux, dev preset):** verify only `shaders/spirv/` is produced
3. **Unit tests:** `ctest --preset test` -- ShaderFormat utility tests and ShaderManager fallback tests pass
4. **Runtime test (Windows Vulkan):** Launch journey -- ShaderManager logs `preferred format: SPIR-V`, loads from `spirv/` subdirectory. Rendering unchanged.
5. **Runtime test (Windows D3D12):** If D3D12 driver selected, ShaderManager logs `preferred format: DXIL`, loads from `dxil/`. Rendering unchanged.
6. **Fallback test:** Delete `dxil/` directory, relaunch -- ShaderManager falls back to `spirv/`, logs fallback, rendering still works.
7. **Shipping guard test:** Build with shipping preset, remove all shader directories -- verify hard error at startup (no runtime compilation fallback in Shipping)
8. **Lint/tidy:** `tools/lint.py --changed` and `tools/tidy.py --changed` pass
9. **WGSL validation:** Verify `shaders/wgsl/` contains valid `.wgsl` files (slangc compilation succeeds). Runtime loading not tested (no backend).

---

## Further Considerations

1. **Metallib precompilation:** MSL source requires Metal API compilation at shader load time (fast, ~ms). For full zero-overhead shipping, `.metallib` precompilation would need macOS-only `xcrun metal` tooling. Deferring this -- MSL source via SDL_GPU is sufficient for now.
2. **DXIL signing:** Unsigned DXIL may not work on some D3D12 configurations. Slang's DXIL output may need signing via `dxil.dll`. This is a platform-specific concern to validate during testing. SDL_GPU may handle this internally.
3. **Build time impact:** Four targets = 4x shader compilation (mitigated by shared module precompilation). Dev preset defaulting to `spirv` keeps iteration fast. Full bundle is primarily for shipping/CI.
