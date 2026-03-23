# Workspace Guide

## Purpose

This document explains how the repository is organised, what targets currently matter, and how to work with the project without already knowing its history.

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
| `core/` | Foundational primitives — types, identifiers, handles, interned strings, result type, logging, events |
| `app/` | Application lifecycle — `Application`, `EngineRuntime`, `EngineConfig`, layers, subsystem management |
| `gameplay/` | Game framework — `Game`, game states, state machine, gameplay tags |
| `modules/` | Module and plugin system — module loading, registration, registrars |
| `project/` | Project metadata — `ProjectDescriptor`, project file discovery and resolution |
| `assets/` | Asset registry and service layer |
| `maths/` | 3D math utilities |
| `platform/` | Window, input, time, backend config, key/mouse codes (`null/` headless, `sdl3/` production) |
| `rendering/` | Rendering subsystem (see below) |
| `scene/` | ECS scene management, components, serialisation, entity helpers, scene settings |
| `physics/` | Physics subsystem — Jolt-based world, bodies, components, plugin |

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

Use `journey` when changing engine runtime behaviour, scene loading, renderer behaviour, or general gameplay-facing systems.

```powershell
bin\Debug\journey.exe
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
bin\Debug\waypoint.exe validate-assets sandbox\journey\assets
bin\Debug\waypoint.exe validate sandbox\journey\assets\scenes\default_scene.json
bin\Debug\waypoint.exe roundtrip-save sandbox\journey\assets\scenes\default_scene.json bin\Debug\assets\scenes\default_scene_roundtrip.json
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
bin\Debug\wayfinder_render_tests.exe
```

## Code Quality

Wayfinder uses clang-format, a post-format fixup script, and clang-tidy to enforce consistent style and catch bugs early. All tools are pinned to **version 18**.

### Formatting

Two tools work together:

- **`.clang-format`** — Allman braces, 4-space indent, `ColumnLimit: 0` (you control line breaks; the formatter handles indentation and spacing).
- **`tools/format-fixup.py`** — post-processor for patterns clang-format cannot handle: collapsing empty type bodies onto one line (`struct Foo {}`) and moving initialiser-list braces to a new line (Allman style).

### Static Analysis

**`.clang-tidy`** enforces naming conventions and catches common bugs:

| Element | Rule | Example |
|---------|------|---------|
| Types | `CamelCase` | `RenderGraph` |
| Functions / Methods | `CamelCase` | `Initialise()` |
| Private / Protected members | `m_` prefix | `m_window` |
| Public members | `CamelCase` (no prefix) | `Position` |
| Local variables / Parameters | `camelBack` | `frameCount` |
| Constants / Enum values | `UPPER_CASE` | `MAX_ENTITIES` |
| Abstract classes | `I` prefix | `ILogger` |
| Template parameters | `T` prefix | `TResource` |
| Namespaces | `CamelCase` | `Rendering` |

Enabled check families: `bugprone-*`, `cppcoreguidelines-*`, `modernize-*`, `performance-*`, `readability-*`, `clang-analyzer-*`, `misc-*`.

### Local Workflow

`tools/lint.py` is the unified entry point:

```powershell
# Check all source files (no modifications)
python tools/lint.py

# Fix formatting in-place
python tools/lint.py --fix

# Only files changed vs main
python tools/lint.py --changed

# Also run clang-tidy (requires a Clang build)
python tools/lint.py --tidy

# Override compile_commands.json location
python tools/lint.py --tidy --build-dir build/clang
```

To generate `compile_commands.json` for clang-tidy:

```powershell
cmake --preset dev-clang
```

### Pre-commit Hooks

**Option A** — `pre-commit` framework (recommended if you have Python):

```powershell
pip install pre-commit
pre-commit install
```

**Option B** — standalone script (zero dependencies):

```powershell
python tools/install-hooks.py            # install
python tools/install-hooks.py --uninstall # remove
```

Both options run `lint.py --fix --staged` before each commit, formatting only the files you are committing. clang-tidy is **not** run in the hook (too slow) — it runs in CI.

### Suppressing Warnings

Use sparingly and with a justification:

```cpp
// NOLINTNEXTLINE(bugprone-narrowing-conversions) — SDL API requires int
int width = static_cast<int>(m_width);

void LegacyCallback(void* data)  // NOLINT(modernize-use-trailing-return-type)
{
    ...
}
```

### CI Pipeline

The GitHub Actions CI workflow (`.github/workflows/ci.yml`) runs four parallel jobs on every push and PR to `main`:

| Job | Runner | What it checks |
|-----|--------|---------------|
| **Format Check** | ubuntu-latest | clang-format + format-fixup (all files) |
| **Build & Test (Linux)** | ubuntu-latest | Clang build + CTest |
| **Build & Test (Windows)** | windows-latest | MSVC build + CTest |
| **Static Analysis** | ubuntu-latest | clang-tidy on changed files (report-only for now) |

CPM dependency sources are cached between runs to speed up builds.

## Recommended Reading Order

If you are new to the repository, read the docs in this order:

1. `README.md`
2. `docs/project_vision.md`
3. `docs/runtime_architecture.md`
4. `docs/data_authoring_and_editor.md`
5. `docs/render_features.md`
6. `docs/sdl3_migration_plan.md`
7. `docs/implementation_plan.md`