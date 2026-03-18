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

**Verification:**
- `journey` renders a lit or unlit cube through SDL_GPU
- Camera from authored scene data drives the view
- The RenderFrame extraction path is still the only scene→renderer boundary

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

**Verification:**
- Authored material assets drive shader and parameter selection at runtime
- Different entities can have different materials/shaders in the same scene
- `waypoint` validates the new material format
- Save/load roundtrips preserve new material fields

---

### Stage 6: Multi-Pass Rendering And Render Targets

**Goal:** The renderer can execute multiple passes per frame with intermediate render targets.

**Tasks:**

1. **Render target management**
   - Create `src/rendering/RenderTarget.h/.cpp`
   - Wrap `SDL_CreateGPUTexture` for color and depth render targets
   - Support creation, resizing (on window resize), and cleanup

2. **Depth buffer**
   - Create a depth texture for the main scene pass
   - Configure pipeline depth/stencil state

3. **Multi-pass execution**
   - `RenderPipeline` executes the extracted pass schedule with real render targets
   - Scene pass renders to a color + depth target
   - Debug pass renders to the same target or a separate overlay target
   - A final composition pass presents to the swapchain

4. **RenderFrame pass data expansion**
   - Passes carry render target configuration (which targets to write, clear values, load/store actions)
   - This is where future post-processing and deferred passes will plug in

5. **Backend capabilities update**
   - `RenderBackendCapabilities` now reports real GPU-backed values: multiple views, render target support, etc.
   - Remove the old single-view / no-render-target limits

**Verification:**
- Scene renders with depth testing through a real depth buffer
- Debug overlays render in a separate pass
- Frame presents correctly through the composition pass
- `RenderBackendCapabilities` reports accurate GPU-backed limits

---

### Stage 7: Cleanup And Stabilization

**Goal:** Remove all Raylib vestiges, stabilize the new baseline, and update tooling.

**Tasks:**

1. **Code cleanup**
   - Remove all Raylib-era type conversions, compatibility shims, and dead code
   - Remove `RenderBackend::Raylib` and `PlatformBackend::Raylib` enum values
   - Clean up any remaining `#include "raylib.h"` references (there should be none by this point)

2. **Null backend update**
   - Ensure the Null/headless backend matches the new interface surface
   - Headless render pipeline tests exercise the pass schedule against the null backend

3. **Test coverage**
   - Headless tests for pipeline creation, material resolution, pass scheduling, and backend capability enforcement
   - Extend existing `waypoint` validation to cover new asset formats

4. **Documentation finalization**
   - Update `runtime_architecture.md` to describe the final post-migration state
   - Update `workspace_guide.md` build commands if they changed
   - Archive or remove this migration plan document once complete

5. **ImGui integration preparation**
   - Verify Dear ImGui SDL3 + SDL_GPU backend compiles and initializes
   - This does not need to be wired into Cartographer yet — just confirm the path works

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
    Application.h/.cpp          (updated bootstrap)
  core/
    BackendConfig.h             (updated enums)
    Game.h/.cpp                 (unchanged)
    ServiceLocator.h/.cpp       (updated to wire SDL3 services)
    ...
  platform/
    sdl3/
      SDL3Window.h/.cpp         (new)
      SDL3Input.h/.cpp          (new)
      SDL3Time.h/.cpp           (new)
    Window.h                    (interface unchanged)
    Input.h                     (interface unchanged)
    Time.h                      (interface unchanged)
  rendering/
    sdl_gpu/
      SDLGPUDevice.h/.cpp       (new — device + swapchain)
      SDLGPURenderAPI.h/.cpp    (new — draw command implementation)
      SDLGPUContext.h/.cpp      (new — frame lifecycle)
    null/
      NullGraphicsContext.h/.cpp (updated interface)
      NullRenderAPI.h/.cpp      (updated interface)
    GPUBuffer.h/.cpp            (new)
    GPUPipeline.h/.cpp          (new)
    Mesh.h/.cpp                 (new — GPU-backed mesh)
    Material.h/.cpp             (new — runtime GPU material)
    RenderTarget.h/.cpp         (new)
    ShaderManager.h/.cpp        (new)
    GraphicsContext.h           (redesigned interface)
    RenderAPI.h                 (redesigned interface)
    RenderFrame.h/.cpp          (expanded)
    RenderPipeline.h/.cpp       (expanded)
    Renderer.h/.cpp             (updated orchestration)
    RenderResources.h/.cpp      (updated resolution)
    SceneRenderExtractor.h/.cpp (updated submissions)
  scene/
    ...                         (unchanged)
  assets/
    ...                         (unchanged)
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
