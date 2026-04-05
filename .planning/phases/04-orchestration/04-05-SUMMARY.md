# Plan 04-05 Summary: AppBuilder State/Overlay/UI Registration Extensions

## Commit
- `98fa9ed` - feat(04-05): AppBuilder state/overlay/UI registration extensions

## What was done
- Created `StateManifest.h` with `StateEntry` (Type, Factory, Capabilities), `InitialState`, `FlatTransitions`, `PushableStates`, `StateUIFactories`
- Created `OverlayManifest.h` with `OverlayEntry` (Type, Factory, Descriptor)
- Extended `AppBuilder.h` with Phase 4 registration API:
  - `AddState<T>(CapabilitySet)` - register application states
  - `SetInitialState<T>()` - designate initial state
  - `AddTransition<TFrom, TTo>()` - declare valid flat transitions
  - `AllowPush<T>()` - declare pushable states
  - `RegisterOverlay<T>(OverlayDescriptor)` - register overlays
  - `ForState<T>().SetUI<U>()` - per-state IStateUI registration via nested `StateBuilder`
- Extended `AppBuilder.cpp` `Finalise()` to produce `StateManifest` and `OverlayManifest` in `AppDescriptor`
- Added 11 test cases in `AppBuilderTests.cpp` covering all new APIs

## Key decisions
- Used `insert_or_assign` instead of `operator[]` for `m_stateEntries` because `StateEntry` contains `std::type_index` (no default constructor)
- StateManifest/OverlayManifest are plain aggregate structs stored as `AppDescriptor` outputs via the existing type-erased `AddOutput` pattern
- `StateBuilder` is a public nested template class inside `AppBuilder` for the fluent `ForState<T>().SetUI<U>()` API

## Files changed
- `engine/wayfinder/src/app/StateManifest.h` (NEW)
- `engine/wayfinder/src/app/OverlayManifest.h` (NEW)
- `engine/wayfinder/src/app/AppBuilder.h` (MODIFIED)
- `engine/wayfinder/src/app/AppBuilder.cpp` (MODIFIED)
- `engine/wayfinder/CMakeLists.txt` (MODIFIED)
- `tests/plugins/AppBuilderTests.cpp` (MODIFIED)

## Test results
- All 4 test suites pass (core, render, scene, physics)
- 11 new test cases all green
