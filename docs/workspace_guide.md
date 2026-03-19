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
| `tools/beacon/`, `expedition/`, `navigator/`, `surveyor/` | Future tools |
| `tests/` | Engine tests |
| `cmake/` | `WayfinderCommon.cmake` (flags/definitions), `WayfinderDependencies.cmake` (FetchContent) |


### Engine

- `engine/wayfinder/` contains the core engine library
- the engine is where application bootstrap, runtime scene ownership, ECS integration, rendering wrappers, asset loading, and core utilities live

### Sandboxes

- `sandbox/journey/` is the main runtime sandbox used to validate engine changes
- `sandbox/waystone/` is a separate runtime shell that exists in the tree but is not the primary day-to-day target yet

### Applications

- `apps/cartographer/` is reserved for the future editor
- `apps/compass/` is reserved for the future project manager

These directories describe intended products, not mature implementations.

### Tools

- `tools/waypoint/` is the first active standalone tool and currently provides asset and scene validation workflows
- `tools/surveyor/`, `tools/expedition/`, `tools/beacon/`, and `tools/navigator/` are reserved for future diagnostics, orchestration, and headless workflows

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
cmake -S . -B build -DWAYFINDER_BUILD_SANDBOX=ON -DWAYFINDER_BUILD_TOOLS=ON
cmake --build build --config Debug --target journey waypoint
```

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

The project currently pulls in or expects the following major libraries:

- `SDL3` for windowing, input, events, and GPU rendering (via SDL_GPU) — replacing Raylib
- `flecs` for ECS and scene world management
- `tomlplusplus` for authored data
- `nlohmann/json` for generated and interchange data
- `spdlog` for logging
- `JoltPhysics` as the intended near-term 3D physics path
- `Box2D`, `Tracy`, and `ImGui` as available dependencies that are not yet part of the main checked-in workflow

Note: Raylib was the original platform and rendering backend. The engine is migrating to SDL3 + SDL_GPU. See `docs/sdl3_migration_plan.md` for the full plan.

## Recommended Reading Order

If you are new to the repository, read the docs in this order:

1. `README.md`
2. `docs/project_vision.md`
3. `docs/runtime_architecture.md`
4. `docs/data_authoring_and_editor.md`
5. `docs/sdl3_migration_plan.md`
6. `docs/implementation_plan.md`