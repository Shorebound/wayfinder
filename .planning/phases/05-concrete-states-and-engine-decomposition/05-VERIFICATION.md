---
phase: 05-concrete-states-and-engine-decomposition
verified: 2026-04-05T20:30:00Z
status: human_needed
score: 5/5 must-haves verified
re_verification: false
human_verification:
  - test: "Build and run full test suite"
    expected: "cmake --build --preset debug succeeds; ctest --preset test reports all 760+ tests passing with 0 failures"
    why_human: "Cannot run cmake/ctest in verification environment (requires Windows build toolchain and GPU-dependent dependencies)"
  - test: "PerformanceOverlay visual output"
    expected: "When running Journey sandbox with ImGui enabled, a small 'Performance' window appears at (10,10) showing FPS as integer and frame time in ms with 2 decimal places, refreshing at ~4Hz"
    why_human: "Visual output requires running application with GPU and ImGui context"
  - test: "EditorState ImGui docking skeleton"
    expected: "When entering EditorState with ImGui enabled, DockSpaceOverViewport creates the root docking space over the main viewport"
    why_human: "Visual ImGui docking requires running application with GPU"
---

# Phase 5: Concrete States and Engine Decomposition Verification Report

**Phase Goal:** V2 architecture handles real workloads -- GameplayState runs simulation, EngineRuntime is split into independent AppSubsystems, rendering uses canvas submission
**Verified:** 2026-04-05T20:30:00Z
**Status:** human_needed
**Re-verification:** No -- initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | GameplayState wraps Simulation in the IApplicationState lifecycle and runs flecs world updates through the v2 frame path | VERIFIED | `GameplayState : IApplicationState` in GameplayState.h; `OnEnter` accesses `context.GetStateSubsystem<Simulation>()`, `OnUpdate` delegates to `m_simulation->Update(deltaTime)`. 7 test cases in GameplayStateTests.cpp. OnRender is a documented no-op for Phase 5 (canvas wiring in Phase 6). |
| 2 | EditorState stub enters and exits cleanly, proving the IApplicationState pattern for future editor work | VERIFIED | `EditorState : IApplicationState` in EditorState.h; `OnEnter`/`OnExit` return success `Result<void>`; `OnRender` has `ImGui::DockSpaceOverViewport` guarded by `WAYFINDER_HAS_IMGUI`; 4 test cases in EditorStateTests.cpp. |
| 3 | FpsOverlay renders frame timing data via the OverlayStack, fully replacing FpsOverlayLayer | VERIFIED | `PerformanceOverlay : IOverlay` in PerformanceOverlay.h; `REFRESH_INTERVAL = 0.25f` (4Hz averaging); `OnUpdate` accumulates and computes display values; `OnRender` renders ImGui window with FPS and ms display; test accessors `GetDisplayFps()`/`GetDisplayMs()` verify averaging logic; 5 test cases in PerformanceOverlayTests.cpp. |
| 4 | EngineRuntime is decomposed into individual AppSubsystems (Window, Input, Time, Renderer) each with proper RAII lifecycle and dependency ordering | VERIFIED | Five AppSubsystem types: WindowSubsystem, InputSubsystem, TimeSubsystem, RenderDeviceSubsystem, RendererSubsystem all inherit AppSubsystem, own `unique_ptr` to platform resources, provide typed accessors. RendererSubsystem owns `FrameCanvases` and `BlendableEffectRegistry`. 6 test cases in EngineSubsystemTests.cpp verify registration, ordering, and capability gating. |
| 5 | Render submission uses typed canvas collectors (SceneCanvas, UICanvas, DebugCanvas) with capability-gated render features via SetEnabled() | VERIFIED | Canvas.h defines `SceneCanvas` (meshes, lights, views), `UICanvas` (ImGui data tracking), `DebugCanvas` (lines, boxes, grid), `FrameCanvases` (aggregate with Reset()). Zero ECS/flecs dependency in Canvas.h. 10 test cases in CanvasTests.cpp. 6 test cases in CanvasRenderFeatureTests.cpp verify `SetEnabled()` gating via `RenderOrchestrator`. |

**Score:** 5/5 truths verified

### Deferred Items

Items not yet met but explicitly addressed in later milestone phases.

| # | Item | Addressed In | Evidence |
|---|------|-------------|----------|
| 1 | GameplayState::OnRender no-ops (no canvas extraction in Phase 5) | Phase 6 | Phase 6 SC #2: "Journey sandbox boots through AddPlugin, enters GameplayState, and renders frames" |
| 2 | Sort key computation removed from gameplay SceneRenderExtractor | Phase 6 | Phase 6 goal: "Application runs entirely on v2 architecture" -- renderer integration handles sorting downstream |
| 3 | Material resolution simplified (no AssetService) | Phase 6 | Phase 6 integrates full render pipeline with asset resolution |

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `engine/wayfinder/src/rendering/graph/Canvas.h` | SceneCanvas, UICanvas, DebugCanvas, FrameCanvases types | VERIFIED | 172 lines. All four types with submit/clear methods, capacity-preserving clear. |
| `engine/wayfinder/src/rendering/graph/Canvas.cpp` | Compilation unit | VERIFIED | 191 bytes. Methods inline in header; cpp exists for build system. |
| `engine/wayfinder/src/app/WindowSubsystem.h` | AppSubsystem wrapping Window | VERIFIED | `class WindowSubsystem : public AppSubsystem`, owns `unique_ptr<Window>`. |
| `engine/wayfinder/src/app/InputSubsystem.h` | AppSubsystem wrapping Input | VERIFIED | `class InputSubsystem : public AppSubsystem`, owns `unique_ptr<Input>`. |
| `engine/wayfinder/src/app/TimeSubsystem.h` | AppSubsystem wrapping Time | VERIFIED | `class TimeSubsystem : public AppSubsystem`, owns `unique_ptr<Time>`. |
| `engine/wayfinder/src/app/RenderDeviceSubsystem.h` | AppSubsystem wrapping RenderDevice | VERIFIED | `class RenderDeviceSubsystem : public AppSubsystem`, owns `unique_ptr<RenderDevice>`. |
| `engine/wayfinder/src/app/RendererSubsystem.h` | RendererSubsystem owning Renderer, FrameCanvases, BlendableEffectRegistry | VERIFIED | All three owned: `unique_ptr<Renderer>`, `FrameCanvases m_canvases`, `BlendableEffectRegistry m_blendableEffectRegistry`. |
| `engine/wayfinder/src/gameplay/SceneRenderExtractor.h` | Gameplay-domain ECS-to-canvas extractor | VERIFIED | `class SceneRenderExtractor` with `void Extract(SceneCanvas&)` methods. Takes `flecs::world&` reference. |
| `engine/wayfinder/src/gameplay/SceneRenderExtractor.cpp` | Full extraction logic (views, meshes, lights, post-process) | VERIFIED | 316 lines. ExtractViews, ExtractMeshSubmissions, ExtractLights, ExtractPostProcessVolumes. Zero rendering pipeline includes. |
| `engine/wayfinder/src/gameplay/GameplayState.h` | IApplicationState wrapping Simulation | VERIFIED | `class GameplayState : public IApplicationState`, non-owning `Simulation*`, owns `unique_ptr<SceneRenderExtractor>`. |
| `engine/wayfinder/src/gameplay/GameplayState.cpp` | Full lifecycle implementation | VERIFIED | OnEnter accesses Simulation, creates extractor. OnUpdate delegates. OnExit cleans up. OnRender documented no-op. |
| `engine/wayfinder/src/gameplay/SimulationConfig.h` | Config struct with BootScenePath | VERIFIED | `struct SimulationConfig` with `std::string BootScenePath` and `float FixedTickRate`. |
| `engine/wayfinder/src/app/EditorState.h` | IApplicationState stub for editor | VERIFIED | `class EditorState final : public IApplicationState`, GetName returns "EditorState". |
| `engine/wayfinder/src/app/EditorState.cpp` | ImGui docking skeleton | VERIFIED | `ImGui::DockSpaceOverViewport` guarded by `WAYFINDER_HAS_IMGUI`. |
| `engine/wayfinder/src/app/PerformanceOverlay.h` | IOverlay for FPS/frame time | VERIFIED | `class PerformanceOverlay final : public IOverlay`, REFRESH_INTERVAL = 0.25f, averaging members. |
| `engine/wayfinder/src/app/PerformanceOverlay.cpp` | Accumulation + ImGui rendering | VERIFIED | OnUpdate with frame accumulation and display compute. OnRender with ImGui window guarded by WAYFINDER_HAS_IMGUI. |
| `tests/rendering/CanvasTests.cpp` | Canvas unit tests | VERIFIED | 10 TEST_CASE blocks. |
| `tests/app/EngineSubsystemTests.cpp` | Subsystem registration/ordering/gating tests | VERIFIED | 6 TEST_CASE blocks. |
| `tests/app/GameplayStateTests.cpp` | GameplayState lifecycle tests | VERIFIED | 7 TEST_CASE blocks. |
| `tests/app/EditorStateTests.cpp` | EditorState lifecycle tests | VERIFIED | 4 TEST_CASE blocks. |
| `tests/app/PerformanceOverlayTests.cpp` | Overlay averaging/lifecycle tests | VERIFIED | 5 TEST_CASE blocks. |
| `tests/rendering/CanvasRenderFeatureTests.cpp` | SetEnabled gating tests | VERIFIED | 6 TEST_CASE blocks. |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| RendererSubsystem.h | Canvas.h | Owns FrameCanvases member | WIRED | `#include "rendering/graph/Canvas.h"`, `FrameCanvases m_canvases` member, `GetCanvases()` accessors |
| WindowSubsystem.h | AppSubsystem.h | Inherits | WIRED | `class WindowSubsystem : public AppSubsystem` |
| GameplayState.h | IApplicationState.h | Implements | WIRED | `class GameplayState : public IApplicationState` |
| GameplayState.cpp | Simulation.h | OnEnter accesses, OnUpdate delegates | WIRED | `#include "Simulation.h"`, `context.GetStateSubsystem<Simulation>()`, `m_simulation->Update(deltaTime)` |
| SceneRenderExtractor.h | Canvas.h | Extract outputs to SceneCanvas | WIRED | Forward-declares `SceneCanvas`, cpp includes `rendering/graph/Canvas.h`, Extract takes `SceneCanvas&` |
| SceneRenderExtractor.cpp | (no rendering pipeline headers) | Abstraction boundary | WIRED | Zero matches for `rendering/pipeline/`, `Renderer.h`, `RenderDevice.h` in includes |
| EditorState.h | IApplicationState.h | Implements | WIRED | `class EditorState final : public IApplicationState` |
| PerformanceOverlay.h | IOverlay.h | Implements | WIRED | `class PerformanceOverlay final : public IOverlay` |
| Old SceneRenderExtractor | (coexists) | Not modified | VERIFIED | `rendering/pipeline/SceneRenderExtractor.h` (781 bytes, Mar 29) and `.cpp` (22KB, Apr 1) still present |

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
|----------|--------------|--------|-------------------|--------|
| SceneRenderExtractor.cpp | SceneCanvas (meshes, lights, views) | flecs world ECS queries | Yes -- reads TransformComponent, MeshComponent, RenderableComponent, LightComponent, ActiveCameraStateComponent | FLOWING |
| PerformanceOverlay.cpp | m_displayFps, m_displayMs | OnUpdate deltaTime accumulation | Yes -- computes from real frame deltas | FLOWING |
| GameplayState.cpp OnRender | SceneCanvas | (no data flow yet) | No -- OnRender is a no-op | DEFERRED to Phase 6 |

### Behavioral Spot-Checks

Step 7b: SKIPPED (cannot run cmake/ctest in verification environment -- requires Windows build toolchain with MSVC and platform-specific dependencies). SUMMARYs report all tests passing (760+ tests, 0 failures across 4 executables).

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-----------|-------------|--------|----------|
| REND-02 | 05-01 | Canvas-based render submission model (typed per-frame data collectors) | SATISFIED | SceneCanvas, UICanvas, DebugCanvas, FrameCanvases all implemented with typed submit/clear methods. 10 test cases. |
| REND-01 | 05-02 | EngineRuntime decomposed into individual AppSubsystems | SATISFIED | Five AppSubsystem types (Window, Input, Time, RenderDevice, Renderer) with RAII lifecycle, dependency ordering, capability gating. 6 test cases. |
| STATE-06 | 05-03 | GameplayState wrapping Simulation into IApplicationState lifecycle | SATISFIED | GameplayState implements IApplicationState, wraps Simulation via non-owning pointer, delegates OnUpdate, creates SceneRenderExtractor. 7 test cases. |
| STATE-07 | 05-04 | EditorState stub proving the IApplicationState pattern | SATISFIED | EditorState implements IApplicationState with ImGui docking skeleton, enters/exits cleanly. 4 test cases. |
| OVER-05 | 05-04 | FpsOverlay rewritten from FpsOverlayLayer | SATISFIED | PerformanceOverlay implements IOverlay with 4Hz display-optimised averaging, ImGui rendering. 5 test cases. |
| REND-03 | 05-04 | Render features with capability-gated activation via SetEnabled() | SATISFIED | CanvasRenderFeatureTests verify SetEnabled(false) prevents AddPasses, round-trip, independent toggle. 6 test cases via RenderOrchestrator integration. |

No orphaned requirements found. All 6 phase requirement IDs (STATE-06, STATE-07, OVER-05, REND-01, REND-02, REND-03) are covered by plans and have implementation evidence.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| GameplayState.cpp | 45-55 | OnRender is empty no-op | Info | Documented intentional -- canvas access via RendererSubsystem wired in Phase 6. Not a stub; the extractor and canvas types are proven functional. |
| SimulationConfig.h | 24 | `FixedTickRate = 60.0f` marked @prototype | Info | Future fixed-step support. Does not block any Phase 5 goal. |

No blocker or warning anti-patterns found. No TODO/FIXME/HACK/PLACEHOLDER comments in any Phase 5 files.

### Human Verification Required

### 1. Full Build and Test Suite

**Test:** Run `cmake --build --preset debug` followed by `ctest --preset test --output-on-failure`
**Expected:** Build succeeds with zero errors. All 760+ tests pass across 4 executables (core, render, scene, physics) with 0 failures.
**Why human:** Verification environment cannot execute cmake/ctest (requires MSVC toolchain and platform dependencies).

### 2. PerformanceOverlay Visual Output

**Test:** Run Journey sandbox with ImGui enabled. Observe the top-left corner of the screen.
**Expected:** A small "Performance" window at (10,10) showing FPS as integer and frame time in ms with 2 decimal places, refreshing at approximately 4Hz with stable values.
**Why human:** Visual ImGui rendering requires running the application with GPU and ImGui context.

### 3. EditorState ImGui Docking Skeleton

**Test:** Enter EditorState with ImGui enabled.
**Expected:** DockSpaceOverViewport creates the root docking space over the main viewport. The viewport should be dockable.
**Why human:** Visual ImGui docking requires running the application with GPU.

### Gaps Summary

No gaps found. All 5 roadmap success criteria are verified at the code level. All 6 requirement IDs have implementation evidence with unit tests.

Three items are explicitly deferred to Phase 6 (GameplayState::OnRender canvas wiring, sort key computation, full material resolution). These are documented in both the plan and summary and are confirmed addressed by Phase 6 success criteria.

38 total TEST_CASE blocks were added across 6 test files, covering canvas submission, subsystem registration/ordering/gating, gameplay state lifecycle, editor state lifecycle, overlay averaging, and render feature gating.

---

_Verified: 2026-04-05T20:30:00Z_
_Verifier: Claude (gsd-verifier)_
