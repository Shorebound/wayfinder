# Plan: Slang Build Integration & Shader Port (#57)

## TL;DR

Replace DXC with Slang's `slangc` compiler, port 12 HLSL shaders to idiomatic Slang (combined per-program files + shared modules), and verify rendering parity. This is the foundation — all other Slang work (#142, #143, #58) builds on it.

## Why #57 First

The 4 sub-issues form a dependency chain:
- **#57** (Build + Port) → foundation, zero deps
- **#142** (Runtime Compilation) → depends on #57
- **#143** (Automated Reflection) → depends on #57 + #142
- **#58** (Offline Multi-Target) → depends on #57 + #142

Each is a distinct architectural change. Combining them prevents incremental validation. #57 alone is significant: new SDK, CMake rewrite, 12 shader files restructured into 8 programs + 3 modules, DXC removal.

---

## Phase 1: Slang SDK Dependency

### 1.1 Create `cmake/WayfinderSlang.cmake`

Slang is a prebuilt binary toolchain, not a source library — CPM does not apply. Use `FetchContent` with direct URL to the release archive.

```cmake
# cmake/WayfinderSlang.cmake — Download prebuilt Slang SDK
include(FetchContent)

set(SLANG_VERSION "2026.5.1" CACHE STRING "Slang SDK version")

# Platform detection
if(WIN32)
    set(_SLANG_PLATFORM "windows-x86_64")
    set(_SLANG_EXT "zip")
elseif(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(_SLANG_PLATFORM "macos-aarch64")
    else()
        set(_SLANG_PLATFORM "macos-x86_64")
    endif()
    set(_SLANG_EXT "tar.gz")
else()
    set(_SLANG_PLATFORM "linux-x86_64")
    set(_SLANG_EXT "tar.gz")
endif()

set(_SLANG_FILENAME "slang-${SLANG_VERSION}-${_SLANG_PLATFORM}.${_SLANG_EXT}")
set(_SLANG_URL "https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/${_SLANG_FILENAME}")

message(STATUS "Fetching Slang SDK ${SLANG_VERSION} for ${_SLANG_PLATFORM}")

FetchContent_Declare(slang_sdk
    URL      "${_SLANG_URL}"
    URL_HASH ""  # TODO: add SHA256 on first successful download
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(slang_sdk)

# Expose paths
if(WIN32)
    set(SLANGC_EXECUTABLE "${slang_sdk_SOURCE_DIR}/bin/slangc.exe" CACHE FILEPATH "Path to slangc" FORCE)
else()
    set(SLANGC_EXECUTABLE "${slang_sdk_SOURCE_DIR}/bin/slangc" CACHE FILEPATH "Path to slangc" FORCE)
endif()

set(SLANG_INCLUDE_DIR "${slang_sdk_SOURCE_DIR}/include" CACHE PATH "Slang headers" FORCE)
set(SLANG_LIB_DIR     "${slang_sdk_SOURCE_DIR}/lib"     CACHE PATH "Slang libraries" FORCE)

if(NOT EXISTS "${SLANGC_EXECUTABLE}")
    message(FATAL_ERROR "slangc not found at ${SLANGC_EXECUTABLE} — check SLANG_VERSION and platform")
endif()
message(STATUS "Found slangc: ${SLANGC_EXECUTABLE}")
```

**Key decisions:**
- Single `SLANG_VERSION` variable — bumping is a one-line change.
- `SLANG_INCLUDE_DIR` / `SLANG_LIB_DIR` exposed but unused until #142 links `libslang` for runtime compilation.
- `URL_HASH` left empty initially — fill in after first download. `FetchContent` caches by URL so repeat builds don't re-download.
- The archive layout places `bin/`, `include/`, `lib/` at the root.

### 1.2 Wire into build

Add `include(WayfinderSlang)` in the root `CMakeLists.txt` after `include(WayfinderDependencies)`.

---

## Phase 2: Rewrite Shader Build Pipeline

### 2.1 Replace `cmake/WayfinderShaders.cmake`

The current function compiles one `.vert`/`.frag` file per invocation via DXC. The new function compiles combined `.slang` files (each containing both `VSMain` and `PSMain`) — or module-only files (no entry points, compile to `.slang-module`).

Slangc supports two entry points in one invocation:
```
slangc basic_lit.slang -target spirv -entry VSMain -o basic_lit.vert.spv -entry PSMain -o basic_lit.frag.spv
```
The `-o` binds to the preceding `-entry`, and `[shader("vertex")]`/`[shader("fragment")]` attributes in source provide stage info.

**New `wayfinder_compile_shaders()` signature:**

```cmake
# wayfinder_compile_shaders(
#   TARGET     <cmake-target>
#   PROGRAMS   <list of .slang program files>     (have entry points)
#   MODULES    <list of .slang module files>       (imported by programs)
#   OUTPUT_DIR <dir>
# )
```

**Per PROGRAM file**, generate `add_custom_command`:
```
${SLANGC_EXECUTABLE}
    ${SHADER_SOURCE}
    -target spirv
    -emit-spirv-directly
    -entry VSMain -o ${STAGING_DIR}/${NAME}.vert.spv
    -entry PSMain -o ${STAGING_DIR}/${NAME}.frag.spv
    -I ${MODULE_DIR}
```
`DEPENDS` should include the source file **and** all module files (so module changes trigger recompile).

**Per MODULE file**, generate `add_custom_command`:
```
${SLANGC_EXECUTABLE}
    ${MODULE_SOURCE}
    -o ${STAGING_DIR}/${NAME}.slang-module
```
Precompiled modules aren't strictly needed (programs `import` source files directly), but they speed up incremental builds when multiple programs share the same module. **This is optional for Phase 2 — start without it.**

**Special case — module-only (fullscreen vertex):** The `fullscreen.slang` module exports the `FullscreenTriangle` function but has no entry points itself. Programs importing it inline the function, so `fullscreen.slang` never needs its own `.spv`.

**Output naming stays `<name>.vert.spv` / `<name>.frag.spv`** — ShaderManager loads exactly that pattern. Zero ShaderManager changes.

### 2.2 Update `sandbox/journey/CMakeLists.txt`

Change file glob from `*.vert` / `*.frag` to `*.slang`. Add the module directory as an include path for slangc (via the `-I` flag in the cmake function — already handled above).

### 2.3 Handle `fullscreen` vertex as shared code

The current pipeline uses `VertexShaderName = "fullscreen"` in 4 different programs (composition_blit, chromatic_aberration, colour_grading, vignette). In the combined-file model, each program file contains its own `VSMain` that calls `import fullscreen; ... FullscreenTriangle(vertexId)`. This means each program produces its own `<name>.vert.spv`.

**ShaderManager impact:** Each program's `VertexShaderName` must now be its own program name (since `.vert.spv` is produced per-program), **not** the shared `"fullscreen"`. This requires updating `ShaderProgramDesc.VertexShaderName` in 4 registration sites (see Phase 4).

Alternatively, we could keep `fullscreen.slang` as a standalone vertex-only program that still emits `fullscreen.vert.spv`, and have the fragment-only programs reference it. But that breaks the "one combined file per program" model and means we still have some split files.

**Decision: Combined files. Update the 4 registration sites.** The duplication is trivial (fullscreen triangle is ~5 lines inlined by slangc) and the model is cleaner. Each post-process program is fully self-contained.

---

## Phase 3: Port Shaders — File Structure

### 3.1 New directory layout

```
engine/wayfinder/shaders/
├── modules/                       ← shared Slang modules
│   ├── transforms.slang           ← TransformData (mvp, model)
│   ├── scene_globals.slang        ← SceneGlobals (light params)
│   └── fullscreen.slang           ← FullscreenOutput, FullscreenTriangle()
│
├── basic_lit.slang                ← vertex + fragment (imports transforms, scene_globals)
├── textured_lit.slang             ← vertex + fragment (imports transforms, scene_globals)
├── unlit.slang                    ← vertex + fragment (imports transforms)
├── debug_unlit.slang              ← vertex + fragment (PosColour, imports transforms)
├── fullscreen_copy.slang          ← vertex + fragment (imports fullscreen)
├── chromatic_aberration.slang     ← vertex + fragment (imports fullscreen)
├── colour_grading.slang           ← vertex + fragment (imports fullscreen)
└── vignette.slang                 ← vertex + fragment (imports fullscreen)
```

**12 HLSL files → 8 combined `.slang` programs + 3 modules = 11 files.**

### 3.2 Module definitions

Modules use `module` declarations and export shared types/functions. Programs `import` them by name using string literal syntax to avoid Slang's underscore→hyphen filename translation.

**`modules/transforms.slang`:**
```slang
module transforms;

public struct TransformData
{
    float4x4 mvp;
    float4x4 model;
};
```

**`modules/scene_globals.slang`:**
```slang
module scene_globals;

public struct SceneGlobals
{
    float3 light_direction;
    float  light_intensity;
    float3 light_colour;
    float  ambient;
};
```

**`modules/fullscreen.slang`:**
```slang
module fullscreen;

public struct FullscreenOutput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

/// Generate a fullscreen triangle from SV_VertexID (no vertex buffer).
/// Pipeline should use empty vertex layout and DrawPrimitives(3).
public FullscreenOutput FullscreenTriangle(uint vertexId)
{
    FullscreenOutput output;
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.Position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    // Flip V: Vulkan framebuffer Y is top-down, scene projection is Y-up.
    output.TexCoord = float2(uv.x, 1.0 - uv.y);
    return output;
}
```

### 3.3 Program files — example conversions

**`basic_lit.slang`** (replaces `basic_lit.vert` + `basic_lit.frag`):
```slang
import "modules/transforms";
import "modules/scene_globals";

// --- Vertex stage resources (set 1) ---
[[vk::binding(0, 1)]]
ConstantBuffer<TransformData> transform;

// --- Fragment stage resources (set 3) ---
struct MaterialParams { float4 base_colour; };
[[vk::binding(0, 3)]]
ConstantBuffer<MaterialParams> material;

[[vk::binding(1, 3)]]
ConstantBuffer<SceneGlobals> globals;

// --- Inter-stage ---
struct VSOutput
{
    float4 Position : SV_Position;
    float3 Normal   : TEXCOORD0;
    float3 Colour   : TEXCOORD1;
};

struct VSInput
{
    float3 Position : TEXCOORD0;
    float3 Normal   : TEXCOORD1;
    float3 Colour   : TEXCOORD2;
};

[shader("vertex")]
VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.Position = mul(transform.mvp, float4(input.Position, 1.0));
    output.Normal   = normalize(mul((float3x3)transform.model, input.Normal));
    output.Colour   = input.Colour;
    return output;
}

[shader("fragment")]
float4 PSMain(VSOutput input) : SV_Target
{
    float3 N = normalize(input.Normal);
    float3 L = normalize(-globals.light_direction);
    float NdotL = max(dot(N, L), 0.0);

    float3 diffuse  = globals.light_colour * globals.light_intensity * NdotL;
    float3 lighting = diffuse + globals.ambient;
    float3 albedo   = input.Colour * material.base_colour.rgb;

    return float4(albedo * lighting, material.base_colour.a);
}
```

**`fullscreen_copy.slang`** (replaces `fullscreen.vert` + `fullscreen_copy.frag`):
```slang
import "modules/fullscreen";

// --- Fragment resources (set 2) ---
[[vk::combinedImageSampler]]
[[vk::binding(0, 2)]]
Texture2D<float4> SceneColourTex;

[[vk::combinedImageSampler]]
[[vk::binding(0, 2)]]
SamplerState PointSampler;

[shader("vertex")]
FullscreenOutput VSMain(uint vertexId : SV_VertexID)
{
    return FullscreenTriangle(vertexId);
}

[shader("fragment")]
float4 PSMain(FullscreenOutput input) : SV_Target
{
    return SceneColourTex.Sample(PointSampler, input.TexCoord);
}
```

### 3.4 Syntax changes from HLSL (all files)

| HLSL (current) | Slang (new) | Reason |
|---|---|---|
| `cbuffer Foo : register(b0) { ... }` | `ConstantBuffer<Foo> name;` (struct declared above) | Idiomatic Slang. Cleaner struct reuse via modules. |
| No entry point attribute | `[shader("vertex")]` / `[shader("fragment")]` | Required for slangc auto-deduction + future runtime compilation (#142). |
| `register(b0, space3)` | Removed — `[[vk::binding(N, SET)]]` is sufficient | Slang handles SPIR-V layout from `[[vk::binding()]]` alone when targeting SPIR-V. `register()` is HLSL-specific and redundant. |
| Duplicated structs | `import "modules/..."` | Shared types via modules. |
| Separate `.vert` / `.frag` files | Single `.slang` per program | Both stages in one file with `[shader()]` attributes. |

**Kept unchanged:**
- `[[vk::binding(N, SET)]]` — required for SDL_GPU's binding convention.
- `[[vk::combinedImageSampler]]` — required for SDL_GPU's combined sampler expectation. **Verify early** (see Risks).
- Entry point names `VSMain` / `PSMain` — matching ShaderManager's hardcoded entry strings.
- All SDL_GPU binding set assignments (vertex: set 0/1, fragment: set 2/3).

### 3.5 Full file conversion table

| # | Current HLSL Files | New Slang File | Imports | Notes |
|---|---|---|---|---|
| 1 | `basic_lit.vert` + `basic_lit.frag` | `basic_lit.slang` | transforms, scene_globals | Lit geometry, vertex colour |
| 2 | `textured_lit.vert` + `textured_lit.frag` | `textured_lit.slang` | transforms, scene_globals | Lit geometry, UV + diffuse texture |
| 3 | `unlit.vert` + `unlit.frag` | `unlit.slang` | transforms | PosNormalColour (normal ignored) |
| 4 | `debug_unlit.vert` + _(uses `unlit.frag`)_ | `debug_unlit.slang` | — | PosColour. Gets its own fragment inlined (see below) |
| 5 | `fullscreen.vert` + `fullscreen_copy.frag` | `fullscreen_copy.slang` | fullscreen | Passthrough blit |
| 6 | `fullscreen.vert` + `chromatic_aberration.frag` | `chromatic_aberration.slang` | fullscreen | Post-process |
| 7 | `fullscreen.vert` + `colour_grading.frag` | `colour_grading.slang` | fullscreen | Post-process |
| 8 | `fullscreen.vert` + `vignette.frag` | `vignette.slang` | fullscreen | Post-process |

**`debug_unlit` note:** Currently `debug_unlit.vert` is paired with `unlit.frag` (different vertex!) — they share the fragment logic but the vertex inputs differ (PosColour vs PosNormalColour). In the combined model, `debug_unlit.slang` inlines its own fragment (copy of unlit's PSMain — 3 lines). This is cleaner than a shared fragment module for 3 lines of code.

### 3.6 Delete old files

Remove all 12 `.vert` / `.frag` files from `engine/wayfinder/shaders/` after the port.

---

## Phase 4: C++ Registration Updates

The combined-file model changes which `.spv` files exist on disk. Programs that previously referenced `"fullscreen"` as `VertexShaderName` now load their own vertex SPV.

### 4.1 Shader name mapping

| Registration Site | Current VertexShaderName | New VertexShaderName |
|---|---|---|
| `CompositionPass.cpp` (L21) | `"fullscreen"` | `"fullscreen_copy"` |
| `ChromaticAberrationFeature.cpp` (L53) | `"fullscreen"` | `"chromatic_aberration"` |
| `ColourGradingFeature.cpp` (L72) | `"fullscreen"` | `"colour_grading"` |
| `VignetteFeature.cpp` (L56) | `"fullscreen"` | `"vignette"` |

The following are UNCHANGED (vertex and fragment already share the same name):
- `SceneOpaquePass.cpp`: `"unlit"/"unlit"`, `"basic_lit"/"basic_lit"`, `"textured_lit"/"textured_lit"` → no changes.

### 4.2 Debug pipeline update

`DebugPass.cpp` (L81-82) currently uses:
```cpp
desc.vertexShaderName = "debug_unlit";
desc.fragmentShaderName = "unlit";
```

New (both from `debug_unlit.slang`):
```cpp
desc.vertexShaderName = "debug_unlit";
desc.fragmentShaderName = "debug_unlit";
```

### 4.3 ShaderManager — no changes

`ShaderManager::GetShader()` builds paths as `<name>.<stage>.spv`. The slangc output files match exactly:
- `basic_lit.vert.spv`, `basic_lit.frag.spv` (from `basic_lit.slang`)
- `fullscreen_copy.vert.spv`, `fullscreen_copy.frag.spv` (from `fullscreen_copy.slang`)
- etc.

Entry points remain `VSMain`/`PSMain`. No code changes needed.

---

## Phase 5: Remove DXC Toolchain

1. **Delete `tools/shadercompiler/`** entirely — the bundled DXC distribution (~50 MB).
2. **Remove DXC references from `cmake/WayfinderShaders.cmake`** — `find_program(DXC_EXECUTABLE ...)`, profiles (`vs_6_0`, `ps_6_0`), all DXC comments.
3. **Check for stray DXC references** in docs, comments, CI scripts.

---

## Phase 6: Validate

1. **Configure:** `cmake --preset dev` succeeds — Slang SDK downloads, `SLANGC_EXECUTABLE` found.
2. **Build:** `cmake --build --preset debug` — all 8 programs compile to 16 `.spv` files via slangc.
3. **Run:** Launch `journey` sandbox — visual rendering identical to current.
4. **Test:** `ctest --preset test` — all render + core + scene tests pass.
5. **Lint:** `tools/lint.py --changed` + `tools/tidy.py --changed` — clean.
6. **Verify no DXC:** grep for `dxc`/`DXC` in `cmake/`, `tools/`, docs — zero hits.
7. **Verify SPV set:** `bin/Debug/assets/shaders/` contains 16 `.spv` files (8 programs × 2 stages).

---

## Implementation Order

```
1. cmake/WayfinderSlang.cmake           (SDK download, SLANGC_EXECUTABLE)
2. cmake/WayfinderShaders.cmake         (rewrite for slangc, new function signature)
3. CMakeLists.txt                       (include WayfinderSlang)
4. engine/wayfinder/shaders/modules/    (3 module files)
5. engine/wayfinder/shaders/*.slang     (8 program files — do fullscreen_copy first to validate)
6. sandbox/journey/CMakeLists.txt       (update globs)
7. Build + run fullscreen_copy ONLY     (validates: slangc works, [[vk::combinedImageSampler]], modules, SPV output)
8. Port remaining 7 programs
9. C++ registration updates             (4 VertexShaderName + 1 FragmentShaderName change)
10. Delete old .vert/.frag files
11. Delete tools/shadercompiler/
12. Full validation pass
```

**Step 7 is the critical gate.** If `[[vk::combinedImageSampler]]` doesn't work, or slangc produces different SPIR-V layout, or modules fail to resolve — we catch it on a single file before porting everything.

---

## Relevant Files

### New files
- `cmake/WayfinderSlang.cmake` — SDK download via FetchContent
- `engine/wayfinder/shaders/modules/transforms.slang`
- `engine/wayfinder/shaders/modules/scene_globals.slang`
- `engine/wayfinder/shaders/modules/fullscreen.slang`
- `engine/wayfinder/shaders/basic_lit.slang`
- `engine/wayfinder/shaders/textured_lit.slang`
- `engine/wayfinder/shaders/unlit.slang`
- `engine/wayfinder/shaders/debug_unlit.slang`
- `engine/wayfinder/shaders/fullscreen_copy.slang`
- `engine/wayfinder/shaders/chromatic_aberration.slang`
- `engine/wayfinder/shaders/colour_grading.slang`
- `engine/wayfinder/shaders/vignette.slang`

### Modified files
- `cmake/WayfinderShaders.cmake` — full rewrite (DXC → slangc)
- `CMakeLists.txt` — add `include(WayfinderSlang)`
- `sandbox/journey/CMakeLists.txt` — update shader globs
- `engine/wayfinder/src/rendering/passes/CompositionPass.cpp` — VertexShaderName `"fullscreen"` → `"fullscreen_copy"`
- `engine/wayfinder/src/rendering/passes/ChromaticAberrationFeature.cpp` — VertexShaderName `"fullscreen"` → `"chromatic_aberration"`
- `engine/wayfinder/src/rendering/passes/ColourGradingFeature.cpp` — VertexShaderName `"fullscreen"` → `"colour_grading"`
- `engine/wayfinder/src/rendering/passes/VignetteFeature.cpp` — VertexShaderName `"fullscreen"` → `"vignette"`
- `engine/wayfinder/src/rendering/passes/DebugPass.cpp` — FragmentShaderName `"unlit"` → `"debug_unlit"`

### Deleted files
- `engine/wayfinder/shaders/*.vert` (5 files)
- `engine/wayfinder/shaders/*.frag` (7 files)
- `tools/shadercompiler/` (entire directory)

---

## Decisions & Rationale

| Decision | Rationale |
|---|---|
| **Combined `.slang` files** (one per program, both stages) | Idiomatic Slang. One file = one shader program. Matches `[shader()]` attr model. Cleaner than 12 disconnected files. |
| **Slang modules** for shared types | Eliminates struct duplication. `transforms`, `scene_globals`, `fullscreen` are the natural shared boundaries. |
| **`ConstantBuffer<T>`** over `cbuffer` | Slang-native syntax. Named struct enables reuse via modules. `cbuffer` works but is HLSL compat only. |
| **Drop `register()` annotations** | Redundant when `[[vk::binding()]]` is present and target is SPIR-V. Less noise. |
| **Prebuilt SDK via FetchContent** (not CPM) | Slang ships prebuilt — building from source requires LLVM. CPM is for source deps. FetchContent handles URL archives natively. |
| **Pin to v2026.5.1** | Latest stable release (2026-05-25). CalVer. Single variable for bumping. |
| **`-emit-spirv-directly`** | Uses Slang's native SPIR-V backend instead of routing through glslang. Faster, better debug info support. |
| **`import "modules/transforms"`** (string literal) | Avoids Slang's underscore→hyphen filename translation (e.g. `import scene_globals` would look for `scene-globals.slang`). String literals use the path as-is. |
| **Separate `modules/` subdir** | Clean separation of shared types from program files. Maps naturally to `-I` include path. |
| **Fullscreen vertex inlined per program** (not shared SPV) | Each post-process program is self-contained. The fullscreen triangle is 5 lines — duplication is negligible, and the model is simpler. |
| **`debug_unlit.slang` gets its own fragment** | Current pairing (`debug_unlit.vert` + `unlit.frag`) crosses file boundaries. Inlining 3 lines of unlit fragment logic keeps the combined-file model clean. |
| **SPIR-V only** | Multi-target (MSL, DXIL, WGSL) is #58. |
| **ShaderResourceCounts stays manual** | Reflection automation is #143. |
| **Entry points `VSMain`/`PSMain`** | ShaderManager hardcodes these. Consistent across all programs. |

---

## Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| **`[[vk::combinedImageSampler]]` not supported in Slang** | 5 programs use it (all texture samplers) | Test `fullscreen_copy.slang` first (step 7). Fallback: Slang's built-in `Sampler2D` combined type, or separate binding slots with `SamplerState`+`Texture2D` as separate bindings. |
| **`[[vk::binding(N, SET)]]` layout differs from DXC** | Rendering broken — wrong resources bound | Compare `spirv-dis` output of DXC vs slangc for `fullscreen_copy`. Bindings must match exactly. |
| **Module import path resolution** | slangc can't find modules at build time | `-I ${SHADER_MODULE_DIR}` in cmake command. Test with one program first. |
| **FetchContent download fails in CI** | Build breaks | Cache the SDK directory by `SLANG_VERSION` hash. `FETCHCONTENT_FULLY_DISCONNECTED` for offline re-builds. |
| **Slang version breaks on update** | Silent regression | Pinned version. CalVer. Add SHA256 hash to `URL_HASH` after first download for integrity. |
| **Two entry points in one slangc invocation** | Might not work for all programs | Tested syntax: `slangc foo.slang -target spirv -entry VSMain -o foo.vert.spv -entry PSMain -o foo.frag.spv` — confirmed by Slang docs ("Working with Multiples"). Fallback: two separate invocations per program. |

---

## Out of Scope (follow-up issues)

- **#142:** Runtime compilation via Slang API (`libslang` linking, `ISession`, hot-reload).
- **#143:** Automated reflection — replace manual `ShaderResourceCounts` with Slang's `ProgramLayout` reflection API.
- **#58:** Multi-target output — MSL, DXIL, WGSL from same `.slang` sources.
- **Module precompilation** — `.slang-module` binaries for faster incremental builds. Nice-to-have once shader count grows.
- **CI caching** — FetchContent cache for Slang SDK. Needed but not blocking this PR.