---
phase: 05-concrete-states-and-engine-decomposition
plan: 02
subsystem: app
tags: [subsystem-registry, appsubsystem, capability-gating, dependency-ordering, FrameCanvases, BlendableEffectRegistry]

# Dependency graph
requires:
  - phase: 02
    provides: SubsystemRegistry with topological ordering, capability-gated activation, SubsystemManifest
  - phase: 05-01
    provides: FrameCanvases aggregate (SceneCanvas, UICanvas, DebugCanvas)
provides:
  - WindowSubsystem wrapping Window with Presentation capability gating
  - InputSubsystem wrapping Input (always active)
  - TimeSubsystem wrapping Time (always active)
  - RenderDeviceSubsystem wrapping RenderDevice, depends on WindowSubsystem
  - RendererSubsystem owning Renderer, FrameCanvases, and BlendableEffectRegistry
  - Dependency ordering: Window -> RenderDevice -> Renderer
  - Capability gating tests verifying headless mode exclusion
affects: [05-03, 05-04, phase-06]

# Tech tracking
tech-stack:
  added: []
  patterns: [AppSubsystem wrapper pattern with RAII lifecycle, ConfigService dependency for init config, proxy-subsystem test pattern]

key-files:
  created:
    - engine/wayfinder/src/app/WindowSubsystem.h
    - engine/wayfinder/src/app/WindowSubsystem.cpp
    - engine/wayfinder/src/app/InputSubsystem.h
    - engine/wayfinder/src/app/InputSubsystem.cpp
    - engine/wayfinder/src/app/TimeSubsystem.h
    - engine/wayfinder/src/app/TimeSubsystem.cpp
    - engine/wayfinder/src/app/RenderDeviceSubsystem.h
    - engine/wayfinder/src/app/RenderDeviceSubsystem.cpp
    - engine/wayfinder/src/app/RendererSubsystem.h
    - engine/wayfinder/src/app/RendererSubsystem.cpp
    - tests/app/EngineSubsystemTests.cpp
  modified:
    - engine/wayfinder/CMakeLists.txt
    - tests/CMakeLists.txt

key-decisions:
  - "Subsystems access EngineConfig via ConfigService AppSubsystem (context.GetAppSubsystem<ConfigService>().Get<EngineConfig>()) rather than constructor injection"
  - "BlendableEffectRegistry owned by RendererSubsystem with no global SetActiveInstance call (per D-07 design decision)"
  - "Proxy subsystem test pattern: lightweight stubs with lifecycle logging matching real descriptor patterns for headless capability/ordering tests"

patterns-established:
  - "AppSubsystem wrapper: owns unique_ptr to platform/rendering resource, RAII lifecycle, typed accessor, config via ConfigService"
  - "Proxy subsystem testing: test stubs with matching descriptors (capabilities, dependencies) and lifecycle logging for headless verification"

requirements-completed: [REND-01]

# Metrics
duration: 21min
completed: 2026-04-05
---

# Phase 05 Plan 02: Engine Subsystem Decomposition Summary

**Five AppSubsystem wrappers (Window, Input, Time, RenderDevice, Renderer) with dependency ordering, capability-gated activation, and RendererSubsystem owning FrameCanvases + BlendableEffectRegistry**

## Performance

- **Duration:** 21 min
- **Started:** 2026-04-05T07:52:29Z
- **Completed:** 2026-04-05T08:13:26Z
- **Tasks:** 2
- **Files modified:** 13

## Accomplishments
- Five independent AppSubsystem types decomposing EngineRuntime's ownership model
- RendererSubsystem owns FrameCanvases (from Plan 01) and BlendableEffectRegistry (no global singleton)
- Dependency chain Window -> RenderDevice -> Renderer enforced via SubsystemDescriptor.DependsOn
- Headless mode: Window/RenderDevice/Renderer excluded by capability gating; Input/Time always active
- Six test cases with 38 assertions covering registration, ordering, and capability gating

## Task Commits

Each task was committed atomically:

1. **Task 1: Five AppSubsystem types with dependency ordering** - `2751294` (feat)
2. **Task 2: Subsystem registration and capability gating tests** - `9f42bb4` (test)

## Files Created/Modified
- `engine/wayfinder/src/app/WindowSubsystem.h` - AppSubsystem wrapping Window, Presentation capability
- `engine/wayfinder/src/app/WindowSubsystem.cpp` - Window creation via ConfigService config, RAII shutdown
- `engine/wayfinder/src/app/InputSubsystem.h` - AppSubsystem wrapping Input, no capabilities required
- `engine/wayfinder/src/app/InputSubsystem.cpp` - Input creation via platform backend config
- `engine/wayfinder/src/app/TimeSubsystem.h` - AppSubsystem wrapping Time, no capabilities required
- `engine/wayfinder/src/app/TimeSubsystem.cpp` - Time creation, GetDeltaTime accessor
- `engine/wayfinder/src/app/RenderDeviceSubsystem.h` - AppSubsystem wrapping RenderDevice, Rendering capability
- `engine/wayfinder/src/app/RenderDeviceSubsystem.cpp` - RenderDevice creation, init via WindowSubsystem dependency
- `engine/wayfinder/src/app/RendererSubsystem.h` - AppSubsystem owning Renderer, FrameCanvases, BlendableEffectRegistry
- `engine/wayfinder/src/app/RendererSubsystem.cpp` - Renderer creation via RenderDeviceSubsystem dependency
- `tests/app/EngineSubsystemTests.cpp` - Registration, ordering, and capability gating tests
- `engine/wayfinder/CMakeLists.txt` - Added 10 new source files to App section
- `tests/CMakeLists.txt` - Added EngineSubsystemTests.cpp to core tests

## Decisions Made
- Subsystems access EngineConfig via ConfigService (context.GetAppSubsystem<ConfigService>().Get<EngineConfig>()) for consistency with the v2 architecture pattern. This keeps configuration access uniform across all subsystems.
- BlendableEffectRegistry is a member of RendererSubsystem with no global SetActiveInstance call, per design decision D-07. Access path: EngineContext -> RendererSubsystem -> GetBlendableEffectRegistry().
- Tests use proxy subsystem types with lifecycle logging (matching real descriptor patterns) rather than attempting to initialize real subsystems. This enables headless testing of registry behavior without platform/GPU dependencies.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
- CMake configure failed in worktree due to dirty CPM cache for SDL3_image (missing git submodules for vendored zlib/libpng). Resolved by running `git submodule update --init --recursive` in the SDL3_image thirdparty directory.
- Worktree branch was based on wrong commit (main instead of feature branch HEAD). Resolved via git reset --soft to correct base.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Five AppSubsystem types ready for use by Phase 05 Plans 03-04 and eventual Phase 6 EngineRuntime replacement
- RendererSubsystem provides the FrameCanvases ownership that subsequent plans need for canvas-based render submission
- No blockers for subsequent work

## Self-Check: PASSED

All 11 created files verified present on disk. Both task commits (2751294, 9f42bb4) verified in git history. 449 core tests pass with 0 failures.

---
*Phase: 05-concrete-states-and-engine-decomposition*
*Completed: 2026-04-05*
