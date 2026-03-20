# Workspace Guide

## Purpose

This document explains how the repository is organized, what targets currently matter, and how to work with the project without already knowing its history.

## Repository Layout

| Path | Purpose |
|------|---------|
| `engine/wayfinder/` | Engine library (static by default) |
| `sandbox/journey/` | Primary playable sandbox + sample assets |
| `sandbox/waystone/` | Separate runtime shell (not yet active) |
| `apps/cartographer/` | Editor (future) |
| `apps/compass/` | Project manager (future) |
| `tools/waypoint/` | Asset validation CLI (active) |
| `apps/beacon/` | Launcher (future) |
| `tools/expedition/`, `tools/navigator/`, `tools/surveyor/` | Future tools |
| `tests/` | Engine tests |
| `cmake/` | `WayfinderCommon.cmake` (flags/definitions), `WayfinderDependencies.cmake` (CPM), `GetCPM.cmake` (bootstrap) |


### Engine

- `engine/wayfinder/` contains the core engine library
- the engine is where application bootstrap, runtime scene ownership, ECS integration, rendering wrappers, asset loading, and core utilities live

Engine source lives under `engine/wayfinder/src/` and is organised by domain:

| Directory | Purpose |
|-----------|---------|
| `core/` | Application lifecycle, engine runtime, events, logging, modules, gameplay tags, identifiers, fundamental types |
| `assets/` | Asset registry and service layer |
| `maths/` | 3D math utilities |
| `platform/` | Window, input, and time abstractions (`null/` headless, `sdl3/` production) |
| `rendering/` | Rendering subsystem (see below) |
| `scene/` | ECS scene management, components, serialisation, entity helpers |

The rendering subsystem is further organised into subdirectories:

| Directory | Purpose |
|-----------|---------|
| `rendering/backend/` | GPU device abstraction — `RenderDevice`, GPU buffers, pipelines, vertex formats, plus backend implementations (`null/`, `sdl_gpu/`) |
| `rendering/graph/` | Render graph framework — `RenderGraph`, features, frame data, intents, sort keys |
| `rendering/pipeline/` | Pipeline execution — `Renderer`, `RenderPipeline`, `RenderContext`, `PipelineCache`, scene extraction |
| `rendering/resources/` | GPU resource management — `RenderResources`, transient buffer and resource pools |
| `rendering/materials/` | Material and shader system — `Material`, parameters, post-process volumes, shader programs and manager |
| `rendering/mesh/` | Geometry data — `Mesh`, vertex formats |
| `rendering/RenderTypes.h` | Rendering-specific types — camera, textures, samplers, render passes, device info |

### Sandboxes

- `sandbox/journey/` is the main runtime sandbox used to validate engine changes
- `sandbox/waystone/` is a separate runtime shell that exists in the tree but is not the primary day-to-day target yet

### Applications

- `apps/cartographer/` is reserved for the future editor
- `apps/compass/` is reserved for the future project manager

These directories describe intended products, not mature implementations.

### Tools

- `tools/waypoint/` is the first active standalone tool and currently provides asset and scene validation workflows
- `tools/surveyor/`, `tools/expedition/`, and `tools/navigator/` are reserved for future diagnostics, orchestration, and headless workflows
- `apps/beacon/` is reserved for the future launcher

### Supporting Areas

- `docs/` contains project documentation
- `tests/` is reserved for automated testing work
- `samples/` and `games/` are for future content and integration targets
- `cmake/` contains shared CMake logic and dependency setup

## Build Targets

The top-level CMake project defines several named targets and feature toggles.

Targets that matter today:

- `wayfinder` is the engine library
- `journey` is the current sandbox executable
- `waypoint` is the current command-line tool, available when `WAYFINDER_BUILD_TOOLS=ON`

Targets that are present conceptually but not active product surfaces yet:

- `cartographer`
- `compass`
- `waystone`
- `surveyor`
- `expedition`
- `beacon`
- `navigator`

## Build Options

Important CMake options:

- `WAYFINDER_BUILD_SANDBOX` defaults to `ON`
- `WAYFINDER_BUILD_TOOLS` defaults to `OFF`
- `WAYFINDER_BUILD_EDITOR`, `WAYFINDER_BUILD_RUNTIME`, `WAYFINDER_BUILD_SAMPLES`, and `WAYFINDER_BUILD_TESTS` are currently opt-in and mostly future-facing
- `WAYFINDER_BUILD_SHARED_LIBS` controls whether the engine is built as a shared library

Typical local setup for runtime and asset work:

```powershell
# Using presets (recommended)
cmake --preset dev          # configures sandbox + tools + tests
cmake --build --preset debug

# Or manually
cmake -S . -B build -DWAYFINDER_BUILD_SANDBOX=ON -DWAYFINDER_BUILD_TOOLS=ON
cmake --build build --config Debug --target journey waypoint
```

See `CMakePresets.json` at the repo root for available presets:

| Preset | What it enables |
|--------|----------------|
| `dev` | Sandbox + Tools + Tests |
| `dev-all` | Everything (sandbox, tools, tests, editor, runtime, samples) |
| `ci` | Same as dev-all, plus warnings-as-errors |
| `shipping` | Sandbox only, optimised |

## Current Working Workflow

### Runtime Iteration

Use `journey` when changing engine runtime behavior, scene loading, renderer behavior, or general gameplay-facing systems.

```powershell
build\bin\Debug\journey.exe
```

The sandbox copies its checked-in asset directory into the output folder after build so it can boot with local assets.

### Asset And Scene Validation

Use `waypoint` when you want to validate authored data without launching the runtime window.

Supported commands today:

- `waypoint validate-assets <asset-root>`
- `waypoint validate <scene-path>`
- `waypoint roundtrip-save <scene-path> <output-path>`

Examples:

```powershell
build\bin\Debug\waypoint.exe validate-assets sandbox\journey\assets
build\bin\Debug\waypoint.exe validate sandbox\journey\assets\scenes\default_scene.toml
build\bin\Debug\waypoint.exe roundtrip-save sandbox\journey\assets\scenes\default_scene.toml build\bin\Debug\assets\scenes\default_scene_roundtrip.toml
```

## Dependency Summary

All third-party dependencies are fetched via [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake). The bootstrap script `cmake/GetCPM.cmake` downloads CPM at configure time; no manual installation is needed. Set `CPM_SOURCE_CACHE` to a shared directory to avoid re-downloading across builds.

Current dependencies:

- `SDL3` for windowing, input, events, and GPU rendering (via SDL_GPU)
- `flecs` for ECS and scene world management
- `tomlplusplus` for authored data
- `nlohmann/json` for generated and interchange data
- `spdlog` for logging
- `JoltPhysics` as the intended near-term 3D physics path
- `ImGui` for immediate-mode UI (editor integration ready)
- `Box2D` and `Tracy` as available dependencies that are not yet part of the main checked-in workflow
- `doctest` for unit testing (linked only when `WAYFINDER_BUILD_TESTS=ON`)

## Running Tests

Tests are built when `WAYFINDER_BUILD_TESTS=ON` (included in the `dev` preset). The test executable runs all rendering and engine tests headlessly using `NullDevice`.

```powershell
# Run via CTest
ctest --preset test

# Or directly
build\bin\Debug\wayfinder_render_tests.exe
```

## Recommended Reading Order

If you are new to the repository, read the docs in this order:

1. `README.md`
2. `docs/project_vision.md`
3. `docs/runtime_architecture.md`
4. `docs/data_authoring_and_editor.md`
5. `docs/render_features.md`
6. `docs/sdl3_migration_plan.md`
7. `docs/implementation_plan.md`