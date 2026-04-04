---
phase: 01-foundation-types
plan: 02
subsystem: app, plugins, gameplay
tags: [v2-interfaces, vocabulary-types, service-provider, capabilities]

requires:
  - "Tag/TagContainer/TagRegistry renamed (01-01)"
  - "NativeTag in Wayfinder namespace (01-01)"
provides:
  - "IApplicationState interface with Result<void> lifecycle"
  - "IOverlay interface with attach/detach lifecycle"
  - "IPlugin interface with Build(AppBuilder&) pattern"
  - "IStateUI interface with state-scoped lifecycle"
  - "AppSubsystem scope base class"
  - "StateSubsystem scope base class"
  - "ServiceProvider concept and StandaloneServiceProvider"
  - "Capability NativeTag constants (Simulation, Rendering, Presentation, Editing)"
  - "CapabilitySet type alias for TagContainer"
affects: [03-app-builder, 04-orchestration, 05-simulation, all downstream consumers of v2 types]

tech-stack:
  added: []
  patterns:
    - "Forward-declared EngineContext/EventQueue in v2 interfaces - no coupling to concrete types"
    - "ServiceProvider concept with static_assert validation at definition site"
    - "NativeTag-based capability constants with self-registration linked list"
    - "CapabilitySet as TagContainer alias - no new container type"

key-files:
  created:
    - engine/wayfinder/src/app/IApplicationState.h
    - engine/wayfinder/src/app/IOverlay.h
    - engine/wayfinder/src/app/AppSubsystem.h
    - engine/wayfinder/src/app/StateSubsystem.h
    - engine/wayfinder/src/plugins/IPlugin.h
    - engine/wayfinder/src/plugins/IStateUI.h
    - engine/wayfinder/src/plugins/ServiceProvider.h
    - engine/wayfinder/src/gameplay/Capability.h
    - tests/app/ApplicationStateTests.cpp
    - tests/plugins/PluginInterfaceTests.cpp
    - tests/plugins/ServiceProviderTests.cpp
  modified:
    - engine/wayfinder/CMakeLists.txt
    - tests/app/SubsystemTests.cpp
    - tests/gameplay/TagTests.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "IPlugin in Wayfinder namespace (not Wayfinder::Plugins) for v2 top-level consistency"
  - "ServiceProvider concept validates against int as representative type"
  - "StandaloneServiceProvider stores non-owning void* references, callers own lifetimes"
  - "CapabilitySet is a plain TagContainer alias, not a distinct type"

patterns-established:
  - "V2 interfaces use forward-declared EngineContext and EventQueue"
  - "Failable lifecycle methods return Result<void>, per-frame methods are void with empty defaults"
  - "ServiceProvider concept with static_assert at definition site"

requirements-completed: [STATE-01, PLUG-01, OVER-01, UI-01, SUB-01, SIM-02, SIM-04, CAP-01]

duration: ~8min
completed: 2026-04-04
---

# Phase 01 Plan 02: V2 Vocabulary Types - Summary

**Created 8 v2 engine headers defining the core type vocabulary: IApplicationState/IOverlay/IPlugin/IStateUI interfaces, AppSubsystem/StateSubsystem scope bases, ServiceProvider concept with StandaloneServiceProvider, and Capability NativeTag constants with CapabilitySet alias.**

## Performance

- **Completed:** 2026-04-04T17:42:00Z
- **Tasks:** 2/2
- **Files created:** 11
- **Files modified:** 4

## Accomplishments

- Created IApplicationState with Result<void> OnEnter/OnExit and void per-frame lifecycle
- Created IOverlay with attach/detach lifecycle matching IApplicationState pattern
- Created IPlugin with Build(AppBuilder&) only pattern (no OnStartup/OnShutdown)
- Created IStateUI with full state-scoped lifecycle mirroring IOverlay
- Created AppSubsystem and StateSubsystem scope base classes for SubsystemCollection
- Created ServiceProvider concept validated by static_assert at definition site
- Created StandaloneServiceProvider with type-erased Register/Get/TryGet
- Created Capability namespace with 4 NativeTag constants and CapabilitySet alias
- All 4 test suites pass (core, render, scene, physics)

## Task Commits

Each task was committed atomically:

1. **Task 1: Create v2 type headers and update engine CMakeLists.txt** - `36c1e53` (feat)
2. **Task 2: Create and extend tests for v2 vocabulary types** - `c3fa90d` (test)

## Files Created

- `engine/wayfinder/src/app/IApplicationState.h` - V2 application state interface with Result<void> lifecycle
- `engine/wayfinder/src/app/IOverlay.h` - V2 overlay interface with attach/detach lifecycle
- `engine/wayfinder/src/app/AppSubsystem.h` - Application-scoped subsystem base
- `engine/wayfinder/src/app/StateSubsystem.h` - State-scoped subsystem base
- `engine/wayfinder/src/plugins/IPlugin.h` - V2 plugin interface with Build-only pattern
- `engine/wayfinder/src/plugins/IStateUI.h` - V2 per-state UI interface
- `engine/wayfinder/src/plugins/ServiceProvider.h` - ServiceProvider concept and StandaloneServiceProvider
- `engine/wayfinder/src/gameplay/Capability.h` - Capability NativeTag constants and CapabilitySet alias
- `tests/app/ApplicationStateTests.cpp` - IApplicationState and IOverlay mock tests
- `tests/plugins/PluginInterfaceTests.cpp` - IPlugin and IStateUI mock tests
- `tests/plugins/ServiceProviderTests.cpp` - ServiceProvider concept and StandaloneServiceProvider tests

## Files Modified

- `engine/wayfinder/CMakeLists.txt` - Added all 8 new headers to source list
- `tests/app/SubsystemTests.cpp` - Added AppSubsystem and StateSubsystem scoping tests
- `tests/gameplay/TagTests.cpp` - Added Capability tag and CapabilitySet tests
- `tests/CMakeLists.txt` - Added 3 new test files to core test target

## Decisions Made

1. **IPlugin in Wayfinder namespace** - V2 IPlugin lives in `Wayfinder` (not `Wayfinder::Plugins` like v1 Plugin) for consistency with the top-level v2 architecture.
2. **ServiceProvider validates against int** - The concept uses `int` as a representative type for checking Get/TryGet signatures. This is sufficient since the methods are templates.
3. **Non-owning StandaloneServiceProvider** - Stores `void*` references. Callers are responsible for ensuring service lifetimes exceed provider usage. This matches the headless test/tools use case where services are stack-allocated.
4. **CapabilitySet is TagContainer alias** - No new container type. Capability tags are just Tags used in TagContainers, maintaining the existing tag infrastructure.

## Deviations from Plan

None - plan executed exactly as written.

## Known Stubs

None.

## Self-Check: PASSED
