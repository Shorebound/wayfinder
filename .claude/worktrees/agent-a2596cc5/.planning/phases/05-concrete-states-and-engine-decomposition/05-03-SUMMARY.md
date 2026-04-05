---
phase: "05"
plan: "03"
subsystem: gameplay
tags: [gameplay-state, simulation, scene-render-extractor, ecs, canvas]
dependency_graph:
  requires: ["05-01"]
  provides: ["GameplayState", "SceneRenderExtractor (gameplay)", "SimulationConfig"]
  affects: ["06-*"]
tech_stack:
  added: []
  patterns: ["IApplicationState concrete implementation", "ECS-to-canvas abstraction boundary", "non-owning state subsystem access"]
key_files:
  created:
    - engine/wayfinder/src/gameplay/SceneRenderExtractor.h
    - engine/wayfinder/src/gameplay/SceneRenderExtractor.cpp
    - engine/wayfinder/src/gameplay/GameplayState.h
    - engine/wayfinder/src/gameplay/GameplayState.cpp
    - engine/wayfinder/src/gameplay/SimulationConfig.h
    - tests/app/GameplayStateTests.cpp
  modified:
    - engine/wayfinder/CMakeLists.txt
    - tests/CMakeLists.txt
decisions:
  - "Sort key computation removed from gameplay SceneRenderExtractor -- deferred to renderer (Phase 6) since SceneCanvas consumers handle sorting downstream"
  - "OnRender no-ops in Phase 5 (headless) -- canvas access via RendererSubsystem wired in Phase 6"
  - "Material resolution simplified (no AssetService) -- full asset-based material resolution deferred to Phase 6"
metrics:
  duration: "61min"
  completed: "2026-04-05"
requirements:
  - STATE-06
---

# Phase 5 Plan 3: GameplayState and SceneRenderExtractor Migration Summary

GameplayState wraps Simulation in IApplicationState lifecycle with gameplay-domain SceneRenderExtractor extracting ECS data into SceneCanvas without any rendering pipeline dependency.

## What Was Done

### Task 1: SceneRenderExtractor migration to gameplay domain

Created a new `SceneRenderExtractor` in `gameplay/` that reads ECS components (ActiveCameraStateComponent, TransformComponent, MeshComponent, RenderableComponent, LightComponent, BlendableEffectVolumeComponent) and writes to `SceneCanvas` methods (AddView, SubmitMesh, SubmitLight). The class takes a `flecs::world&` reference and is called explicitly by GameplayState (not a flecs system, per D-15).

Key differences from the old `rendering/pipeline/SceneRenderExtractor`:
- Writes to `SceneCanvas` instead of building a `RenderFrame` with layers
- No dependency on `RenderDevice.h`, `SortKey.h`, or any rendering pipeline headers
- Sort key computation deferred to downstream renderer (Phase 6)
- Material resolution simplified (no AssetService dependency)
- Old extractor left completely untouched for coexistence

### Task 2: GameplayState + SimulationConfig + tests (TDD)

Implemented `GameplayState` as the concrete `IApplicationState` that wraps Simulation:
- **OnEnter**: Accesses `Simulation` via `context.GetStateSubsystem<Simulation>()` (non-owning), creates `SceneRenderExtractor` with Simulation's flecs world
- **OnUpdate**: Delegates to `Simulation::Update(deltaTime)` which calls `m_world.progress(deltaTime)`
- **OnRender**: No-ops in Phase 5 (no canvas available headlessly); Phase 6 wires canvas via RendererSubsystem
- **OnExit**: Resets extractor, nulls simulation pointer

`SimulationConfig` provides a `BootScenePath` string and `FixedTickRate` float (prototype) for ConfigService loading.

7 test cases validate the full lifecycle: IApplicationState conformance, name, enter/exit, update delegation to flecs world, and config defaults.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed flecs API usage in gameplay SceneRenderExtractor**
- **Found during:** Task 1
- **Issue:** Used pointer dereference syntax (`*entity.get<T>()`) when flecs `entity::get<T>()` returns a reference, not a pointer
- **Fix:** Changed all `*entityHandle.get<T>()` to `entityHandle.get<T>()`
- **Files modified:** `engine/wayfinder/src/gameplay/SceneRenderExtractor.cpp`
- **Commit:** 00d3687

**2. [Rule 1 - Bug] Fixed incomplete type error for unique_ptr member**
- **Found during:** Task 2
- **Issue:** `GameplayState` header used `~GameplayState() override = default;` but `SceneRenderExtractor` was forward-declared, causing incomplete type error in `unique_ptr` destructor
- **Fix:** Moved constructor/destructor definitions to `.cpp` file where `SceneRenderExtractor` is a complete type
- **Files modified:** `engine/wayfinder/src/gameplay/GameplayState.h`, `engine/wayfinder/src/gameplay/GameplayState.cpp`
- **Commit:** 0344686

**3. [Rule 3 - Blocking] Removed RenderDevice.h / SortKey.h dependency**
- **Found during:** Task 1
- **Issue:** Copying sort key computation from old extractor required `BlendState`/`BlendPresets` from `RenderDevice.h` which violates the abstraction boundary requirement
- **Fix:** Removed sort key computation entirely; SceneCanvas consumers (render features) apply sorting downstream in Phase 6
- **Files modified:** `engine/wayfinder/src/gameplay/SceneRenderExtractor.cpp`
- **Commit:** 00d3687

**4. [Rule 3 - Blocking] Fixed corrupted CPM SDL3_image cache**
- **Found during:** Build setup
- **Issue:** CPM source cache for SDL3_image was corrupted (missing `.git` dir and `CMakeLists.txt`), preventing worktree build configuration
- **Fix:** Re-cloned SDL3_image into the CPM cache and initialized submodules
- **Files modified:** None (build infrastructure only)

## Known Stubs

| File | Line | Stub | Reason |
|------|------|------|--------|
| `GameplayState.cpp` | OnRender | No-op (no canvas extraction) | Canvas access via RendererSubsystem wired in Phase 6 |
| `SimulationConfig.h` | 23 | FixedTickRate = 60.0f | @prototype: future fixed-step support |

These stubs do not prevent the plan's goal (proving IApplicationState lifecycle with Simulation). OnRender extraction is the integration deliverable for Phase 6.

## Test Results

All 744 tests pass across 4 test executables:
- `wayfinder_core_tests`: 456 passed (7 new)
- `wayfinder_render_tests`: 163 passed
- `wayfinder_scene_tests`: 89 passed
- `wayfinder_physics_tests`: 36 passed

## Commits

| Task | Commit | Message |
|------|--------|---------|
| 1 | 00d3687 | feat(05-03): migrate SceneRenderExtractor to gameplay domain |
| 2 (RED) | 14c4758 | test(05-03): add failing tests for GameplayState and SimulationConfig |
| 2 (GREEN) | 0344686 | feat(05-03): implement GameplayState, SimulationConfig, and passing tests |

## Self-Check: PASSED

All 6 created files verified present. All 3 task commits verified in git history.
