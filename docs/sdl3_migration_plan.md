# SDL3 Migration Plan

## Purpose

This document is the tactical plan for migrating Wayfinder from Raylib to SDL3 + SDL_GPU.

The migration replaces the platform layer (window, input, time) and the rendering backend (draw-primitive calls) with a modern, GPU-resource-oriented foundation that can support the project's rendering vision: dynamic lights, multi-pass rendering, render targets, custom shaders, volumetrics, and post-processing.

The architectural layers above the backend — ECS, scene ownership, authored data, extraction, and the pass-based frame model — survive intact.

## Why This Migration

Raylib is a complete high-level renderer, not a platform layer. Wayfinder's `IRenderAPI` wraps Raylib's `DrawBox`, `DrawLine3D`, and `DrawGrid` — there is no access to GPU resources, shader pipelines, render targets, or buffer management underneath. The engine cannot implement any of its visual ambitions (dynamic lighting, volumetrics, time-of-day, post-processing) through this surface.

SDL3 solves the problem cleanly:

- **SDL3 core** provides cross-platform window management, input, events, audio, and gamepad handling — the platform layer the engine actually needs
- **SDL_GPU** provides a modern cross-platform GPU abstraction (Vulkan, D3D12, Metal) designed for engines that want to own their rendering pipeline
- SDL3 is battle-tested, permissive-licensed, and actively maintained
- Dear ImGui has a mature SDL3 + SDL_GPU backend, which directly supports Cartographer editor development

Raylib cannot serve as "just a platform layer" because it tightly couples window creation to its internal OpenGL context. You cannot create a Raylib window and then drive a separate GPU backend through it.

## What Survives

These systems transfer to the new backend without structural changes:

- **Flecs ECS** — scene entity storage, component registration, system scheduling
- **Scene ownership** — `Scene` as world boundary, `SceneDocument` for parsing, validated load/save
- **TOML authoring** — scene documents, prefabs, materials, all validation rules
- **SceneRenderExtractor** — the concept and extraction pattern (ECS → `RenderFrame`)
- **RenderFrame / pass model** — submission-oriented frame data with explicit views and passes
- **Identity system** — UUID-backed typed IDs
- **waypoint** — headless validation (never depended on Raylib)
- **CMake workspace structure** — engine / sandbox / tools / apps / docs layout
- **Engine-owned math types** — `Float3`, `Matrix4`, `Color`, `Camera` value types at the component boundary
- **Logging, Tracy profiling, physics dependencies**

## What Gets Replaced

### Platform Layer

| Current | Replacement |
|---|---|
| `RaylibWindow` | SDL3 window (`SDL_CreateWindow`) |
| `RaylibInput` | SDL3 event-based input (`SDL_PollEvent`) |
| `RaylibTime` | SDL3 timer or engine-owned high-resolution timer |
| `PlatformBackend::Raylib` enum | `PlatformBackend::SDL3` enum |
| `Window::Create(config, PlatformBackend)` | Updated factory creating SDL3 window |

### Rendering Backend

| Current | Replacement |
|---|---|
| `IGraphicsContext` (BeginFrame/EndFrame/Clear) | SDL_GPU device and command buffer lifecycle |
| `IRenderAPI` (DrawBox, DrawLine3D, DrawGrid) | GPU-resource interface: pipeline creation, buffer management, render pass execution, draw commands |
| `RaylibGraphicsContext` | SDL_GPU device wrapper |
| `RaylibRenderAPI` | SDL_GPU rendering implementation |
| `RenderBackend::Raylib` enum | `RenderBackend::SDL_GPU` enum |
| `RenderBackendCapabilities` (MaxViewCount=1, no render targets) | Capabilities reflecting real GPU features |
| Hardcoded box primitives | Vertex/index buffer backed mesh geometry |
| Material = albedo color | Material = shader reference + parameter bindings |

### CMake Dependencies

| Current | Replacement |
|---|---|
| `raylib` FetchContent block | `SDL3` FetchContent block |
| `target_link_libraries(... raylib)` | `target_link_libraries(... SDL3::SDL3)` |
| Raylib source files under `src/platform/raylib/` and `src/rendering/raylib/` | SDL3 source files under `src/platform/sdl3/` and `src/rendering/sdl_gpu/` |

## Migration Stages

The migration is broken into stages that each produce a bootable, testable state. No stage should leave the engine in a state where `journey` cannot run.

---

### Stage 1: SDL3 Platform Layer

**Goal:** Replace Raylib as the window, input, and event provider. The engine boots with an SDL3 window showing a cleared screen.

**Tasks:**

1. **CMake: Add SDL3 dependency**
   - Add SDL3 FetchContent block to `WayfinderDependencies.cmake`
   - Remove the Raylib FetchContent block
   - Update `target_link_libraries` in the engine CMakeLists to link `SDL3::SDL3` instead of `raylib`

2. **Window: Create SDL3Window**
   - Create `src/platform/sdl3/SDL3Window.h/.cpp` implementing `IWindow`
   - `SDL_Init(SDL_INIT_VIDEO)`, `SDL_CreateWindow`, `SDL_DestroyWindow`
   - Update `Window::Create()` factory to produce `SDL3Window`
   - Update `PlatformBackend` enum: replace `Raylib` with `SDL3`

3. **Input: Create SDL3Input**
   - Create `src/platform/sdl3/SDL3Input.h/.cpp` implementing `IInput`
   - Keyboard and mouse state from `SDL_PollEvent` / `SDL_GetKeyboardState`
   - Gamepad support through SDL3's gamepad API

4. **Time: Create SDL3Time or engine-owned timer**
   - Create `src/platform/sdl3/SDL3Time.h/.cpp` implementing `ITime`
   - Use `SDL_GetPerformanceCounter` / `SDL_GetPerformanceFrequency` for high-resolution timing

5. **ServiceLocator: Wire SDL3 services**
   - Update `ServiceLocator::Initialize` to create SDL3 platform services
   - Update `Application` bootstrap to use the new backends

6. **Temporary rendering stub**
   - Create a minimal SDL_GPU device initialization inside the graphics context so the window can clear to a color
   - This validates SDL3 + SDL_GPU coexistence before the full renderer rebuild

7. **Remove Raylib platform files**
   - Delete `src/platform/raylib/` directory
   - Delete Raylib includes from all platform code

**Verification:**
- `journey` boots, shows a cleared SDL3 window, responds to input, shuts down cleanly
- `waypoint` still runs headless validation unchanged
- No Raylib headers remain in platform code

---

### Stage 2: SDL_GPU Device And Frame Lifecycle

**Goal:** Establish the GPU device, swapchain, and frame submission lifecycle. The engine can clear the screen and present frames through SDL_GPU.

**Tasks:**

1. **GPU device creation**
   - Create `src/rendering/sdl_gpu/SDLGPUDevice.h/.cpp`
   - `SDL_CreateGPUDevice` with backend preferences (Vulkan preferred, D3D12 fallback on Windows, Metal on macOS)
   - Claim the SDL3 window for GPU presentation (`SDL_ClaimWindowForGPUDevice`)

2. **Replace IGraphicsContext**
   - Redesign the interface around GPU command buffer lifecycle:
     - `AcquireCommandBuffer()` → begin frame work
     - `AcquireSwapchainTexture()` → get the current back buffer
     - `Submit()` → submit command buffer for execution
   - Implement in terms of `SDL_AcquireGPUCommandBuffer`, `SDL_AcquireGPUSwapchainTexture`, `SDL_SubmitGPUCommandBuffer`

3. **Basic clear-screen pass**
   - Begin a render pass targeting the swapchain texture
   - Clear to the configured background color
   - End pass and submit

4. **Update Application frame loop**
   - Replace BeginFrame/EndFrame calls with the new command-buffer-oriented lifecycle
   - Confirm frame pacing and vsync through SDL_GPU swapchain configuration

5. **Remove Raylib rendering files**
   - Delete `src/rendering/raylib/` directory
   - Remove all Raylib includes from rendering code

6. **Update NullGraphicsContext**
   - Keep the Null backend operational for headless testing
   - Update it to match the new interface shape

**Verification:**
- `journey` clears the screen through SDL_GPU and presents frames at the correct rate
- No Raylib headers remain anywhere in the codebase
- Null backend still works for headless test targets

---

### Stage 3: Shader And Pipeline Infrastructure

**Goal:** The engine can compile shaders and create GPU pipeline state objects.

**Tasks:**

1. **Shader compilation pipeline**
   - Decide on shader source format: HLSL or GLSL with SDL_GPU's cross-compilation, or pre-compiled SPIR-V / DXIL
   - Recommended starting point: HLSL source compiled to SPIR-V (via `shadercross` or offline tooling), since SDL_GPU can consume SPIR-V on all backends
   - Create `src/rendering/ShaderManager.h/.cpp` for loading compiled shader bytecode

2. **Pipeline state creation**
   - Create `src/rendering/GPUPipeline.h/.cpp`
   - Wrap `SDL_CreateGPUGraphicsPipeline` with engine-friendly configuration: vertex layout, shader stages, blend state, depth/stencil state, primitive topology
   - Support caching pipeline objects by configuration hash

3. **Vertex layout definitions**
   - Define the engine's initial vertex formats: position + normal + UV at minimum
   - Register vertex buffer layouts that map to pipeline input descriptions

4. **Basic test pipeline**
   - Create a minimal vertex + fragment shader pair (solid color or unlit textured)
   - Create a pipeline object and bind it during a render pass
   - This does not need to draw geometry yet — the goal is pipeline creation succeeding without errors

**Verification:**
- Shader bytecode loads successfully
- Pipeline objects are created without GPU validation errors
- The test pipeline can be bound during a render pass (even if nothing is drawn)

---

### Stage 4: Geometry And Draw Calls

**Goal:** The engine can render real geometry through vertex and index buffers.

**Tasks:**

1. **Buffer management**
   - Create `src/rendering/GPUBuffer.h/.cpp`
   - Wrap `SDL_CreateGPUBuffer` for vertex and index buffers
   - Support upload via transfer buffers (`SDL_CreateGPUTransferBuffer`, `SDL_UploadToGPUBuffer`)

2. **Mesh representation**
   - Create `src/rendering/Mesh.h/.cpp` (GPU-backed mesh, distinct from the authored `MeshComponent`)
   - A mesh owns a vertex buffer, an index buffer, and draw parameters (index count, vertex count, primitive topology)
   - Create hardcoded test meshes first: unit cube, unit plane, unit sphere

3. **Replace IRenderAPI with draw-command interface**
   - The new interface operates on bound pipelines and buffers rather than named primitive shapes
   - Core operations: `BindPipeline`, `BindVertexBuffer`, `BindIndexBuffer`, `SetUniform` / push constants, `DrawIndexed`
   - These map directly to SDL_GPU command buffer calls

4. **Uniform / push constant support**
   - Model-view-projection matrix as the first uniform
   - Per-object transform as a second uniform or push constant
   - Wire camera data from `RenderFrame` into view/projection matrices

5. **First rendered geometry**
   - Render the unit cube through the full pipeline: shader + pipeline + buffers + uniforms
   - Use the extracted camera from `RenderFrame`

6. **Update SceneRenderExtractor**
   - Submissions now reference a mesh handle (not "draw a box with these dimensions")
   - Material references stay as asset IDs; resolution happens in the renderer

7. **Compute pipeline surface**
   - Extend `RenderDevice` with `BeginComputePass()`, `EndComputePass()`, `BindComputePipeline()`, `DispatchCompute()`
   - Extend `ShaderManager` to load compute shaders (not just vertex/fragment pairs)
   - Implement in `SDLGPUDevice` via `SDL_BeginGPUComputePass`, `SDL_BindGPUComputePipeline`, `SDL_DispatchGPUCompute`
   - Add no-op stubs in `NullDevice`
   - No compute work is dispatched yet — the goal is having the interface ready so that later systems (light clustering, GPU particles, probe updates, GPU culling) do not require a device API retrofit

**Verification:**
- `journey` renders a lit or unlit cube through SDL_GPU
- Camera from authored scene data drives the view
- The RenderFrame extraction path is still the only scene→renderer boundary
- Compute pipeline interface compiles and is callable (exercised by a headless no-op test)

---

### Stage 4.5: Draw Call Infrastructure

**Goal:** Establish the draw call sorting and dynamic buffer allocation strategies that all subsequent rendering work builds on. These are architectural decisions that cost little now but are painful to retrofit later.

**Why now:** Stage 4 gets geometry on screen. Before Stage 5 introduces multiple materials and Stage 6 introduces multiple passes, the submission and allocation patterns need to be in place. Bolting sort keys onto an existing flat draw loop, or adding a transient allocator after debug rendering already creates/destroys buffers per frame, creates unnecessary churn.

**Tasks:**

1. **Sort-key draw call sorting**
   - Assign a 64-bit sort key to each `MeshSubmission` in `RenderFrame`
   - Key layout:
     ```
     [2 bits: layer]  [16 bits: pipeline/material ID]  [32 bits: depth]  [14 bits: sub-sort]
     ```
   - **Layer** encodes broad ordering: `0 = Opaque`, `1 = Transparent`, `2 = Overlay`
   - **Pipeline/material ID** clusters identical pipeline+material combinations to minimize GPU state changes
   - **Depth** is camera-space Z: front-to-back for opaques (early-Z rejection), back-to-front for transparents (correct blending). Encoding flips the bit interpretation per layer
   - **Sub-sort** is a tiebreaker for deterministic ordering (sequence counter or entity ID hash)
   - `SceneRenderExtractor` stamps the sort key at extraction time using the active camera's view matrix
   - `RenderPipeline` (or the future `RenderGraph` pass executor) sorts submissions by key before dispatch — a single `std::sort` on the submission array
   - Immediate benefit: draw calls batch naturally by material, opaques reject efficiently via early-Z, transparents blend correctly

2. **Transient buffer allocator**
   - Create `src/rendering/TransientBufferAllocator.h/.cpp`
   - Purpose: per-frame dynamic vertex/index data (debug lines, debug shapes, grid, future particles and text quads) without per-frame GPU buffer creation/destruction
   - Implementation: a persistent ring buffer (large GPU buffer, e.g. 4 MB) with a per-frame write offset that wraps
     - `Allocate(size_t byteCount) → { GPUBuffer*, offset, size }` — bumps the write offset, returns a sub-region
     - At frame start, reset the write offset (or advance the ring region)
     - Uses SDL_GPU transfer buffers for upload: map → memcpy → unmap → upload to the ring buffer region
   - The allocator owns one vertex ring buffer and one index ring buffer
   - Debug rendering and any other dynamic geometry uses this allocator instead of creating transient GPU buffers
   - Static meshes (unit cube, loaded models) continue using their own dedicated GPU buffers from `GPUBuffer`

3. **Migrate debug geometry to transient allocator**
   - Debug lines, debug cubes, grid rendering write vertex data through the transient allocator
   - Validate that the ring buffer handles multiple debug draw calls per frame without overwriting live data

**Verification:**
- Draw calls are sorted by sort key before dispatch — visible as material batching (no unnecessary pipeline rebinds between identical materials)
- Debug geometry renders correctly through the transient allocator with no per-frame buffer allocations visible in Tracy
- Multiple frames run without ring buffer corruption or overwrite
- Sort key correctness: opaque objects render front-to-back, transparent objects (when added) render back-to-front

---

### Stage 5: Material System

**Goal:** Materials are shader + parameter bindings, not just an albedo color.

**Tasks:**

1. **Engine material model**
   - A material references a shader (or pipeline configuration) and carries parameter bindings (colors, floats, textures)
   - Create `src/rendering/Material.h/.cpp` for the runtime GPU-facing material
   - Distinguish authored material assets (TOML) from runtime GPU material state

2. **Material asset evolution**
   - Extend the TOML material format to reference a shader name and parameter set:
     ```toml
     asset_id = "..."
     asset_type = "material"
     name = "MarkerCubeMaterial"
     shader = "unlit"
     [parameters]
     base_color = [184, 184, 184, 255]
     ```
   - Keep backward compatibility: materials that omit `shader` default to `"unlit"` or `"default"`
   - Update `waypoint` validation to cover the new fields

3. **Material → pipeline binding**
   - `RenderResourceCache` resolves a material asset → runtime material → pipeline + uniform bindings
   - Different shaders may require different pipeline configurations (blend, depth, etc.)

4. **Per-object material binding in draw calls**
   - During pass execution, bind the material's pipeline and set its uniforms before drawing each submission

5. **Unlit and basic lit shaders**
   - `unlit`: vertex color or base_color uniform, no lighting
   - `basic_lit`: single directional light, diffuse shading — enough to validate the light extraction path

6. **Shader variant / permutation system**
   - Shader source files use `#ifdef` blocks for optional features: `ALPHA_TEST`, `SKINNED`, `NORMAL_MAP`, `VERTEX_COLOR`, etc.
   - A `ShaderVariantKey` is a bitmask of enabled feature flags
   - Materials declare which features they need (via the TOML `[features]` table or inferred from parameter bindings)
   - `ShaderManager` compiles and caches permutations on demand: (shader name + variant key) → compiled bytecode
     - DXC already supports `-D` defines — the offline compilation step generates permutations or the manager invokes DXC at load time for development builds
   - Pipeline cache key becomes (shader name + variant key + rasterizer state hash)
   - Start small: the initial variant set might just be `VERTEX_COLOR` on/off and `ALPHA_TEST` on/off. The system is designed to grow as shaders gain features
   - Avoids a proliferation of near-duplicate hand-authored shader files

**Verification:**
- Authored material assets drive shader and parameter selection at runtime
- Different entities can have different materials/shaders in the same scene
- Shader variants compile and cache correctly — two materials using the same shader with different features produce different pipeline objects
- `waypoint` validates the new material format
- Save/load roundtrips preserve new material fields

---

### Stage 6: Render Graph, Multi-Pass, And Extensibility

**Goal:** The renderer executes a dependency-driven render graph each frame, supports intermediate render targets, and exposes a `RenderFeature` API that lets game developers inject custom passes without modifying engine code.

**Why a render graph:** Stages 3–5 deliver shaders, geometry, and materials — the building blocks of a single pass. But the engine's visual ambitions (dynamic lighting, shadows, post-processing, volumetrics, time-of-day) require multiple passes that read and write intermediate textures. A flat pass list cannot express "the shadow map pass must run before the scene pass because the scene pass reads the shadow map." A render graph solves this: passes declare their resource dependencies, the graph resolves execution order, and transient resources are allocated and freed automatically.

**SRP inspiration:** Unity's Scriptable Render Pipeline has three ideas worth adopting:
- **ScriptableRenderPass** — a self-contained pass unit with declared inputs/outputs and an execute callback. Wayfinder's equivalent is `RenderGraphPass`.
- **RenderFeature** — a registerable extension that injects one or more passes into the frame. This is the game-developer-facing extensibility surface.
- **Well-known resource handles** — the engine publishes named handles (`SceneColor`, `SceneDepth`) that features can read from and write to.

What we skip: Unity's replaceable pipeline concept (URP vs HDRP) is for a multi-audience commercial engine. Wayfinder has one rendering philosophy; the pipeline itself is not swappable. The extensibility lives at the pass/feature level.

**Tasks:**

1. **RenderGraph core**
   - Create `src/rendering/RenderGraph.h/.cpp`
   - A `RenderGraph` is built each frame, used once, then discarded
   - API for adding passes:
     ```cpp
     graph.AddPass("MainScene", [&](RenderGraphBuilder& builder) {
         auto depth = builder.CreateTransient(DepthDesc);
         auto color = builder.CreateTransient(ColorDesc);
         builder.WriteColor(color);
         builder.WriteDepth(depth);
         return [=](RenderDevice& device, const RenderGraphResources& res) {
             // bind pipeline, draw scene submissions
         };
     });
     ```
   - `RenderGraphBuilder`: passes declare reads (texture, buffer) and writes (color target, depth target) through typed resource handles
   - `RenderGraphResources`: resolved at execution time, maps handles to actual GPU textures/buffers
   - Execution: topological sort on declared dependencies, then execute passes in resolved order
   - Pass culling: if no subsequent pass reads a pass's output AND the pass does not target the swapchain, the pass is removed

2. **Transient resource management**
   - The graph allocates GPU textures when first written, tracks lifetime across dependent passes, and releases them when last read
   - Create `src/rendering/TransientResourcePool.h/.cpp`
   - Pool recycles textures across frames by matching format/dimensions (avoids per-frame allocation)
   - **Lifetime-aware recycling:** when the graph knows a texture is last read by Pass B, and a later Pass C needs a new transient texture of the same format/size, the pool can return the same physical texture. This is not memory aliasing — it is pool-level reuse guided by the graph's lifetime analysis
   - No memory aliasing or sub-allocation — keep it simple. Profile first, optimize if needed.

3. **Render target and depth buffer support**
   - `SDL_CreateGPUTexture` for color and depth render targets, wrapped in the transient pool
   - Support creation, resizing (on window resize), and cleanup
   - Depth texture for the main scene pass with pipeline depth/stencil state configured

4. **Well-known resource handles**
   - The `Renderer` publishes named resource handles that engine and game code can reference:
     - `SceneColor` — main color target (HDR or LDR depending on configuration)
     - `SceneDepth` — main depth target
     - `Swapchain` — final presentation target (created by the graph itself)
   - These are the stable contracts between engine passes and game-developer features
   - Additional handles (e.g. `ShadowMap`, `GBuffer`) are created by passes that produce them and discovered by passes that consume them

5. **Engine core passes**
   - Rewrite `RenderPipeline` to build a `RenderGraph` each frame instead of iterating a flat pass array
   - The engine registers its built-in passes:
     - **MainScene** — renders opaque geometry to SceneColor + SceneDepth
     - **Debug** — renders debug overlays (grid, wireframes, debug lines) to SceneColor
     - **Composition** — reads SceneColor, writes to Swapchain (fullscreen blit or tonemap)
   - Pass data still comes from `RenderFrame` (submissions, lights, cameras) — the graph consumes the frame data, it does not replace it

6. **RenderFeature extensibility API**
   - Create `src/rendering/RenderFeature.h`
   - A `RenderFeature` is a registerable extension with a single responsibility:
     ```cpp
     class RenderFeature {
     public:
         virtual ~RenderFeature() = default;
         virtual void AddPasses(RenderGraph& graph, const RenderFrame& frame) = 0;
     };
     ```
   - `Renderer` holds a list of registered features, calls `AddPasses()` after adding engine core passes
   - Features inject passes that declare dependencies on well-known handles:
     ```cpp
     class PixelationFeature : public RenderFeature {
         void AddPasses(RenderGraph& graph, const RenderFrame& frame) override {
             graph.AddPass("Pixelation", [&](RenderGraphBuilder& builder) {
                 auto sceneColor = builder.Read(WellKnown::SceneColor);
                 auto output = builder.Write(WellKnown::SceneColor); // in-place
                 return [=](RenderDevice& device, const RenderGraphResources& res) {
                     // downscale + upscale via pixelation shader
                 };
             });
         }
     };
     ```
   - Registration API on the `Renderer`:
     ```cpp
     renderer.AddFeature(std::make_unique<PixelationFeature>());
     renderer.RemoveFeature("Pixelation");
     ```
   - Features can be added at startup or toggled at runtime

7. **Ordering guarantees**
   - Execution order comes from resource dependencies, not registration order
   - For passes that read and write the same resource (e.g. post-process chain), the graph respects declaration order as a tiebreaker
   - If a feature needs to run "after MainScene but before Debug," it reads `SceneColor` (produced by MainScene) and the Debug pass also reads `SceneColor` — the graph handles the sequencing

8. **Compute pass support in the graph**
   - `RenderGraph` supports compute passes in addition to raster passes
   - A compute pass declares buffer/texture reads and writes like a raster pass, but executes via `BeginComputePass` / `DispatchCompute` / `EndComputePass` on the device
   - The topological sort treats compute passes identically — resource dependencies determine ordering regardless of pass type
   - This enables future systems (light clustering, GPU particle updates, probe re-lighting) to participate in the graph without special-casing

9. **Post-processing volume blending**
   - Create `src/rendering/PostProcessVolume.h/.cpp`
   - A `PostProcessVolume` is an authored data structure (TOML component on a scene entity) that carries:
     - A shape: `Global`, `Box`, or `Sphere`
     - A priority (integer, higher wins on tie)
     - A blend distance (world units for spatial fade-in)
     - A partial set of post-processing parameter overrides: exposure, color grading LUT reference, fog density, fog color, bloom threshold, bloom intensity, vignette, etc.
   - A global volume provides defaults; spatial volumes override selectively
   - **Blending at extraction time:** `SceneRenderExtractor` queries all active `PostProcessVolumeComponent` entities, evaluates camera position against each volume's shape and blend distance, and produces a single blended `PostProcessSettings` struct on the `RenderFrame`
   - `RenderFeature` passes (bloom, color grading, fog) read the blended settings from the frame — they do not query volumes themselves
   - This is the primary authoring surface for time-of-day, weather, location-based atmosphere, and mood transitions
   - TOML example:
     ```toml
     [post_process_volume]
     shape = "global"
     priority = 0
     [post_process_volume.overrides]
     exposure = 1.0
     fog_density = 0.02
     fog_color = [180, 200, 220, 255]
     bloom_threshold = 1.5
     ```
   - `waypoint` validates volume components: shape is a known enum, priority is an integer, overrides are within valid ranges
   - Add `PostProcessVolumeComponent` to `ComponentRegistry` with Apply/Serialize/Validate functions

**Verification:**
- Scene renders with depth testing through a real depth buffer
- Debug overlays render in a separate graph pass, correctly ordered after the scene pass
- Frame presents correctly through the composition pass
- A test `RenderFeature` (e.g. a solid-color fullscreen overlay) can be registered and executes in the correct graph position
- Removing the feature restores the default rendering
- Headless tests verify graph construction, topological sort, and pass culling

---

### Stage 7: Cleanup And Stabilization

**Goal:** Remove all Raylib vestiges, stabilize the new baseline, validate the render graph and feature API, and update tooling.

**Tasks:**

1. **Code cleanup**
   - Remove all Raylib-era type conversions, compatibility shims, and dead code paths
   - Remove `RenderBackend::Raylib` and `PlatformBackend::Raylib` enum values if still present
   - Clean up any remaining `#include "raylib.h"` references (there should be none by this point)
   - Audit render graph for unused utility code added during Stage 6 development

2. **Null backend update**
   - Ensure the Null/headless backend matches the full interface surface including render graph resource operations
   - `NullDevice` supports all `RenderGraph` operations as no-ops so the graph can be exercised headlessly

3. **Test coverage**
   - Headless tests for: pipeline creation, material resolution, buffer upload, shader loading
   - Render graph tests: topological sort correctness, pass culling, transient resource allocation/reuse, cycle detection (should error)
   - RenderFeature tests: feature registration, pass injection ordering, feature removal mid-session
   - Extend existing `waypoint` validation to cover new asset formats

4. **Documentation finalization**
   - Document how to create a `RenderFeature` (the primary game-developer extension point)
   - Update `workspace_guide.md` build commands if they changed
   - Archive or remove this migration plan document once complete

5. **ImGui integration preparation**
   - Verify Dear ImGui SDL3 + SDL_GPU backend compiles and initializes
   - This does not need to be wired into Cartographer yet — just confirm the path works

6. **Property-level reflection metadata**
   - Extend `ComponentRegistry` with per-property metadata: property name, type enum (`Float`, `Int`, `Color`, `Vec3`, `String`, `AssetRef`, `Enum`), display name, and optional constraints (min/max, step, enum values)
   - Each component's registration entry gains a `std::span<const PropertyMeta>` describing its editable properties
   - Material parameter bindings also carry `PropertyMeta` so the editor can generate appropriate widgets (color picker for colors, sliders for floats, asset browser for texture references)
   - This is not a full C++ reflection system — it is a hand-authored metadata table per component type, co-located with the existing Apply/Serialize/Validate functions
   - The inspector in Cartographer (when built) generates ImGui widgets automatically from this metadata instead of hand-wiring UI per component
   - `waypoint` can use the same metadata to provide richer validation error messages ("exposure must be between 0.0 and 10.0" instead of "invalid value")

**Verification:**
- Zero references to Raylib in the entire codebase
- All existing headless tests pass
- `journey` renders authored scenes through the full SDL_GPU pipeline
- `waypoint` validates all current asset types
- Clean build with no warnings related to the migration

## Dependency Changes

### Add

```cmake
# SDL3
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.10  # or latest stable at time of migration
)
set(SDL_SHARED OFF CACHE BOOL "Build SDL3 shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build SDL3 static library" FORCE)
set(SDL_TEST OFF CACHE BOOL "Build SDL3 tests" FORCE)
FetchContent_MakeAvailable(SDL3)
```

### Remove

```cmake
# Remove entire Raylib FetchContent block
# Remove raylib from target_link_libraries
```

### Engine link target

```cmake
target_link_libraries(${ENGINE_NAME}
    PUBLIC
      wayfinder_common
      SDL3::SDL3
      flecs::flecs_static
      tomlplusplus::tomlplusplus
)
```

## File Structure After Migration

```
engine/wayfinder/src/
  application/
    Application.h/.cpp            (owns RenderDevice, bootstrap order: Window → Device → Renderer)
  core/
    BackendConfig.h               (SDL3 + SDL_GPU enums)
    Game.h/.cpp                   (unchanged)
    ServiceLocator.h/.cpp         (platform-only: Input, Time)
    ...
  platform/
    sdl3/
      SDL3Window.h/.cpp           (SDL_CreateWindow, GetNativeHandle)
      SDL3Input.h/.cpp            (SDL_PollEvent input)
      SDL3Time.h/.cpp             (SDL_GetPerformanceCounter timing)
    Window.h                      (interface + GetNativeHandle)
    Input.h                       (interface unchanged)
    Time.h                        (interface unchanged)
  rendering/
    sdl_gpu/
      SDLGPUDevice.h/.cpp         (SDL_GPU device, swapchain, command buffer lifecycle)
    null/
      NullDevice.h/.cpp           (headless stub for testing)
    GPUBuffer.h/.cpp              (Stage 4 — vertex/index buffer wrapper)
    GPUPipeline.h/.cpp            (Stage 3 — pipeline state objects)
    Mesh.h/.cpp                   (Stage 4 — GPU-backed mesh)
    TransientBufferAllocator.h/.cpp (Stage 4.5 — ring buffer for per-frame dynamic geometry)
    Material.h/.cpp               (Stage 5 — runtime GPU material, authored TOML parsing)
    ShaderManager.h/.cpp          (Stage 3 — shader bytecode loading + variant permutation cache)
    RenderDevice.h                (abstract GPU device interface, includes compute pipeline surface)
    RenderTypes.h                 (Color, Camera, Float3/Matrix4, GPU enums, descriptors, SortKey)
    RenderGraph.h/.cpp            (Stage 6 — per-frame dependency graph, topological sort, pass culling, compute passes)
    RenderFeature.h               (Stage 6 — extensibility: game-developer pass injection)
    PostProcessVolume.h/.cpp      (Stage 6 — spatial volume blending for post-process parameters)
    TransientResourcePool.h/.cpp  (Stage 6 — pooled transient texture allocation with lifetime-aware recycling)
    RenderFrame.h                 (scene → renderer data: views, passes, submissions, lights)
    RenderPipeline.h/.cpp         (builds RenderGraph each frame from RenderFrame + features)
    Renderer.h/.cpp               (top-level orchestrator, owns features and pipeline)
    RenderResources.h/.cpp        (caching layer for mesh/material resolution)
    RenderIntent.h                (render intent type enum)
    SceneRenderExtractor.h/.cpp   (ECS → RenderFrame extraction)
  scene/
    ...                           (unchanged)
  assets/
    ...                           (unchanged)
```

## Risk Mitigation

**Risk: SDL_GPU is relatively new.**
SDL_GPU shipped with SDL 3.2.0 and has been stable since. It was developed by the MojoShader/FNA author specifically for game engine use. It targets the same three modern APIs (Vulkan, D3D12, Metal) and has been validated in shipping titles. The API is simpler than raw Vulkan while providing the resource control Wayfinder needs.

**Risk: Shader cross-compilation complexity.**
Start with a single shader language (HLSL recommended) and pre-compile to SPIR-V offline. SDL_GPU's `SDL_ShaderCross` can also handle runtime translation. Avoid building a multi-language shader pipeline until there is a proven need.

**Risk: Losing the working sandbox during migration.**
Each stage is designed to end with a bootable `journey`. Stage 1 can use a trivial SDL_GPU clear as its "renderer." The old Raylib backend should be removed entirely at stage boundaries rather than maintained in parallel — parallel backends create more maintenance burden than they save.

**Risk: Scope creep during renderer rebuild.**
The renderer rebuild should target visual parity with the current Raylib baseline first (colored boxes, wireframes, debug lines, grid). Visual ambition (lighting, shadows, post-processing, volumetrics) belongs to Phase 7 of the implementation plan, not to this migration.

**Risk: Web build target (future consideration).**
SDL_GPU currently targets Vulkan, D3D12, and Metal. It does not emit WebGPU. If a browser target becomes a goal, the path is:

1. **Shader language is not the blocker.** HLSL → SPIR-V (current pipeline) can be translated to WGSL via `naga` (Rust, usable as a CLI tool) or `tint` (Google's Dawn tool). The shader authoring workflow does not need to change.
2. **GPU abstraction is the blocker.** SDL_GPU has no WebGPU backend today. The options at that point would be:
   - **Wait for an SDL_GPU WebGPU backend.** SDL's architecture could support one, and it has been discussed in the SDL community, but nothing ships today.
   - **Add a `wgpu-native` backend behind `RenderDevice`.** `wgpu-native` exposes a C API implementing `webgpu.h`. Wayfinder's `RenderDevice` abstraction is already backend-agnostic — adding a `WGPUDevice` implementation alongside `SDLGPUDevice` is the intended extensibility path. This backend would target both native (Vulkan/D3D12/Metal via wgpu) and browser (WebGPU via wasm).
   - **Emscripten + WebGPU directly.** Emscripten can compile C++ to WebAssembly and provides WebGPU bindings. A thin `RenderDevice` implementation against raw `webgpu.h` would be the minimal approach.
3. **Platform layer:** SDL3 already compiles to Emscripten (web). Window, input, and event handling transfer.
4. **Practical advice:** Do not design for web now. The `RenderDevice` abstraction already provides the seam. When a browser target is needed, add a backend — do not restructure the engine.

## Success Criteria

The migration is complete when:

1. Raylib is fully removed from the codebase and dependency tree
2. `journey` renders authored scenes through SDL_GPU
3. The `RenderFrame` extraction model remains the scene→renderer boundary
4. Materials reference shaders and bind parameters through the GPU pipeline
5. The renderer supports render targets and multi-pass execution
6. `waypoint` headless validation works unchanged
7. Null/headless backend works for automated tests
8. Dear ImGui SDL3 backend is verified to compile (ready for Cartographer)


I just ran journey.exe. It seems like everything is upside down.

Add in extensibility for PostProcessing. Developers should be able to add in post processing as well if they haven't got it.

SDL_Shadercross usage