---
plan: 02
status: complete
commit: 81f3a53
requirements: [STATE-02, STATE-03, STATE-04, STATE-08]
---

# Plan 04-02: ApplicationStateMachine - Summary

## What was done
- **ApplicationStateMachine.h/.cpp**: Full state orchestration engine with:
  - Type-indexed state registration via `AddState<T>(capabilities)`
  - Build-time graph validation in `Finalise()` (BFS reachability, transition target checks)
  - Deferred flat transitions (`RequestTransition<T>` + `ProcessPending`)
  - Push/pop modal stack with suspend/resume lifecycle
  - Push negotiation via `ComputeBackgroundPolicy` (AND intersection of preferences + policy)
  - IStateUI factory registration and lifecycle mirroring (attach/detach/suspend/resume)
  - LifecycleHookManifest integration (FireStateEnter/FireStateExit)
  - Last-write-wins for multiple pending operations in a single frame
- **LifecycleHooks.h**: Added non-template `type_index` overloads for `FireStateEnter`/`FireStateExit`
- **tests/app/ApplicationStateMachineTests.cpp**: 19 test cases covering:
  - Finalise validation (valid graph, missing initial, missing target, unreachable)
  - Start lifecycle, flat transitions, deferred execution
  - Push/pop (suspend/resume, not exit/enter for persisted states)
  - Background policy negotiation
  - Last-write-wins, IStateUI lifecycle, lifecycle hooks, shutdown, capabilities

## Files changed
- `engine/wayfinder/src/app/ApplicationStateMachine.h` (new)
- `engine/wayfinder/src/app/ApplicationStateMachine.cpp` (new)
- `engine/wayfinder/src/plugins/LifecycleHooks.h` (modified)
- `engine/wayfinder/CMakeLists.txt` (modified)
- `tests/app/ApplicationStateMachineTests.cpp` (new)
- `tests/CMakeLists.txt` (modified)
