---
phase: 02-subsystem-infrastructure
plan: 02
status: complete
started: 2026-04-04
completed: 2026-04-04
---

# Plan 02-02 Summary: EngineContext v2 and EngineContextRef

## What Was Built

Replaced the 27-line EngineContext aggregate struct with a non-owning facade class providing typed subsystem access, Phase 4 transition stubs, and graceful shutdown support. Created EngineContextRef flecs singleton component and ComputeEffectiveCaps utility.

### Key Features
- **Typed subsystem access**: `GetAppSubsystem<T>()`, `TryGetAppSubsystem<T>()`, `GetStateSubsystem<T>()`, `TryGetStateSubsystem<T>()` with assert-on-null and nullable variants
- **Phase 4 stubs**: `RequestTransition<T>()`, `RequestPush<T>()`, `RequestPop()`, `ActivateOverlay()`, `DeactivateOverlay()` with WAYFINDER_ASSERT stubs
- **Graceful shutdown**: `RequestStop()` / `IsStopRequested()` flag
- **Partial construction**: Default-constructible with nullptr registries for headless tests
- **Capability computation**: `ComputeEffectiveCaps()` free function computes union of app and state capability sets
- **ECS singleton**: `EngineContextRef` trivial flecs component replaces static GameSubsystems accessor
- **Const correctness**: Full const overloads for all subsystem access methods

### Files

key-files:
  created:
    - engine/wayfinder/src/app/EngineContext.cpp
    - engine/wayfinder/src/gameplay/EngineContextRef.h
  modified:
    - engine/wayfinder/src/app/EngineContext.h
    - engine/wayfinder/src/app/EngineRuntime.h
    - engine/wayfinder/src/app/EngineRuntime.cpp
    - engine/wayfinder/CMakeLists.txt
    - tests/app/EngineRuntimeTests.cpp
    - tests/app/SubsystemTests.cpp

## Deviations

- **flecs world.get<T>() returns reference, not pointer**: Plan expected `world.get<T>()` to return a pointer. Flecs returns `const T&`. Used `world.has<T>()` for existence checks instead of nullptr comparison.
- **TestEngineRuntime fixture simplified**: Changed `GetContext()` to return `EngineContext{}` instead of `Runtime->BuildContext()` since EngineContext is now default-constructible.
- **BuildContext removed entirely**: No other callers existed besides the test. Removed from EngineRuntime.h, EngineRuntime.cpp, and the corresponding test case.

## Test Results

14 new test cases across 3 suites:

**EngineContext (6 tests):**
- Default null registries, SetAppSubsystems wiring, SetStateSubsystems wiring
- TryGet nullptr when registry null, RequestStop/IsStopRequested, const access

**ComputeEffectiveCaps (5 tests):**
- Union of app and state capabilities, empty app caps, empty state caps
- Both empty returns empty, shared capabilities not duplicated

**EngineContextRef (3 tests):**
- Set and get flecs singleton, remove clears access, not set by default

Full test suite: 4/4 test executables pass, 318 test cases in core, 1173 assertions, 0 regressions.

## Self-Check: PASSED
