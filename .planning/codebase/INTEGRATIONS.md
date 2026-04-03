# External Integrations

**Last mapped:** 2026-04-03
**Focus areas:** core, app, plugins, gameplay/simulation

---

## Platform Abstraction Layer

The engine abstracts platform services behind interfaces with backend implementations.

### Window

| Interface | Implementations | Files |
|-----------|-----------------|-------|
| `Window` | SDL3Window (primary), NullWindow (headless) | `engine/wayfinder/src/platform/Window.h` |
| | | `engine/wayfinder/src/platform/sdl3/SDL3Window.cpp` |
| | | `engine/wayfinder/src/platform/null/NullWindow.h` |

### Input

| Interface | Implementations | Files |
|-----------|-----------------|-------|
| `Input` | SDL3Input (keyboard/mouse), NullInput (stub) | `engine/wayfinder/src/platform/Input.h` |
| | | `engine/wayfinder/src/platform/sdl3/SDL3Input.cpp` |
| | | `engine/wayfinder/src/platform/null/NullInput.h` |

### Time

| Interface | Implementations | Files |
|-----------|-----------------|-------|
| `Time` | SDL3Time (system clock), NullTime (stub) | `engine/wayfinder/src/platform/Time.h` |
| | | `engine/wayfinder/src/platform/sdl3/SDL3Time.cpp` |
| | | `engine/wayfinder/src/platform/null/NullTime.h` |

### Backend Selection

Currently enum-based via `BackendConfig.h`:
- `PlatformBackend`: SDL3 or Null
- `RenderBackend`: SDL_GPU or Null

**v2 direction:** Backend selection becomes plugin composition (e.g. `app.AddPlugin<SDLWindowPlugin>()`) instead of enum dispatch. See `docs/plans/application_architecture_v2.md`.

---

## GPU / Rendering Backend

### SDL_GPU Abstraction

SDL_GPU provides a cross-platform GPU abstraction over:
- **Vulkan** (Linux, Windows fallback)
- **Direct3D 12** (Windows preferred)
- **Metal** (macOS/iOS)
- **OpenGL** (fallback)

Key integration files:
- `engine/wayfinder/src/rendering/backend/RenderDevice.h` - abstract GPU interface
- `engine/wayfinder/src/rendering/backend/sdl_gpu/SDLGPUDevice.h` - ~1400 lines, full SDL_GPU impl
- `engine/wayfinder/src/rendering/backend/null/NullDevice.h` - headless stub for tests

The engine's `RenderDevice` interface wraps SDL_GPU operations: buffer creation, texture management, shader loading, pipeline state, render passes, draw calls, compute dispatch.

### Shader Compiler Integration (Slang)

Slang SDK integration provides:
- **Build-time compilation**: `.slang` -> `.spv` via `slangc` invoked from CMake
- **Runtime compilation**: `Slang::slang` library for hot-reload (when shader source dir configured)
- **Reflection**: Runtime query of shader bindings, uniforms, push constants

Key files:
- `cmake/WayfinderSlang.cmake` - SDK download + binding
- `cmake/WayfinderShaders.cmake` - build-time compilation function
- `engine/wayfinder/src/rendering/materials/SlangCompiler.h` - runtime compiler wrapper
- `engine/wayfinder/src/rendering/materials/SlangReflection.h` - reflection queries

---

## ECS Framework (Flecs)

Flecs is the core data model. Engine wraps it lightly:

- `engine/wayfinder/src/ecs/Flecs.h` - include wrapper (suppresses MSVC warnings)
- Systems registered via `PluginRegistry::RegisterSystem()` -> `SystemRegistrar::ApplyToWorld()`
- Components registered via `RuntimeComponentRegistry` (serialisation-capable)
- World singletons (e.g. `ActiveGameState`) for cross-system communication

**Current usage:** `Game` (soon `Simulation`) owns the `flecs::world`. Systems are lambda-based. Plugin registration follows the registrar pattern.

**v2 direction:** ECS system registration moves to `AppBuilder::RegisterSystem()`. `Simulation` stays as flecs world owner but becomes framework-agnostic via `ServiceProvider` concept.

---

## Physics

### Jolt Physics (Primary - 3D)

Integration via `PhysicsPlugin`:
- `engine/wayfinder/src/physics/PhysicsPlugin.cpp` - plugin registration
- `engine/wayfinder/src/physics/PhysicsWorld.h` - Jolt world wrapper
- `engine/wayfinder/src/physics/PhysicsSubsystem.h` - `GameSubsystem` lifecycle
- `engine/wayfinder/src/physics/JoltUtils.h` - type conversions

Uses Jolt's own allocator and job system. Bodies/shapes created via Jolt API, synced to ECS transforms via systems.

### Box2D (Optional - 2D)

Available as fallback for 2D physics scenarios. Less deeply integrated than Jolt.

**v2 direction:** `PhysicsSubsystem` becomes a `StateSubsystem` with `RequiredCapabilities = { Capability::Simulation }` and `DependsOn = { typeid(AssetService) }`.

---

## Diagnostics & Profiling

### Tracy (CPU/GPU Profiling)

- Controlled by `WAYFINDER_ENABLE_PROFILING` CMake option
- Wrapper macros: `WAYFINDER_PROFILE_SCOPE()`, `WAYFINDER_PROFILE_FRAME_MARK()` (`core/Profiling.h`)
- No-ops when disabled
- GPU markers integrated with render passes

### spdlog (Logging)

- Category-based: `LogEngine`, `LogRenderer`, `LogInput`, `LogAudio`, `LogAssets`, `LogPhysics`, `LogGame`, `LogScene`
- Compile-time format validation via `std::format_string`
- Configurable verbosity per category
- Output targets: console, file (via `ILogOutput`)

### Dear ImGui (Debug UI)

- Wrapped with SDL3/SDL_GPU backend
- Used for debug overlays, inspector panels
- `FpsOverlayLayer` is the current debug layer
- **v2 direction:** ImGui remains for debug/editor; game UI will use a dedicated toolkit

---

## File I/O & Assets

### File System

- Uses `std::filesystem` (C++17 standard)
- No custom file system abstraction
- Asset loading goes through `AssetService` (`engine/wayfinder/src/assets/AssetService.h`)

### Asset Pipeline

| Format | Handler | Purpose |
|--------|---------|---------|
| JSON scenes | `SceneDocument` | Entity/component serialisation |
| JSON materials | `Material::Load()` | Material definitions |
| JSON textures | `AssetService` | Texture asset descriptors |
| Binary mesh | `Mesh::LoadFromFile()` | Custom binary mesh format |
| TOML config | `EngineConfig::LoadFromFile()` | Engine configuration |
| TOML tags | `GameplayTagRegistry::LoadTagFile()` | Gameplay tag definitions |

**Waypoint** (asset CLI tool) handles import from external formats:
- glTF 2.0 (via fastgltf) -> engine mesh/material/texture
- Mesh optimisation (via meshoptimizer)
- Tangent generation (via MikkTSpace)

---

## External Services

**None currently.** The engine has no network, cloud, or external API integrations. It is fully offline/local.

---

## v2 Integration Changes

Per `docs/plans/application_architecture_v2.md`, the integration layer will change:

| Current | v2 | Notes |
|---------|-----|-------|
| `Window::Create()` factory | `SDLWindowPlugin` registers `SDL3Window : Window : AppSubsystem` | Backend = plugin composition |
| `Input::Create()` factory | `SDLInputPlugin` registers subsystem | Same pattern |
| `EngineRuntime` owns all services | Decomposed into individual `AppSubsystem` instances | Each service registered independently |
| `BackendConfig` enum dispatch | Plugin set IS the capability manifest | No enum needed |
| `BlendableEffectRegistry` static instance | Becomes standalone `AppSubsystem` | DI, no global |
| `GameplayTagRegistry : GameSubsystem` | Becomes `AppSubsystem` (app lifetime) | Accessible via `ServiceProvider` in Simulation |
