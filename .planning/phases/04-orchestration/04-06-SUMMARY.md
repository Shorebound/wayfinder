# Plan 04-06 Summary: EngineContext Wiring + Integration Tests

## Commit
- `57be6d8` - feat(04-06): wire EngineContext to ASM and OverlayStack, integration tests

## What was done
- Replaced all Phase 4 assert stubs in EngineContext with real delegation:
  - `RequestTransition<T>()` delegates to `m_stateMachine->template RequestTransition<T>()`
  - `RequestPush<T>()` delegates to `m_stateMachine->template RequestPush<T>()`
  - `RequestPop()` delegates to `m_stateMachine->RequestPop()`
  - `ActivateOverlay()` delegates to `m_overlayStack->Activate(overlayType, *this)`
  - `DeactivateOverlay()` delegates to `m_overlayStack->Deactivate(overlayType, *this)`
- Added `SetStateMachine(ApplicationStateMachine*)` and `SetOverlayStack(OverlayStack*)` setters
- Uncommented `m_stateMachine` and `m_overlayStack` private member pointers
- Removed all `@prototype Phase 4` comments from EngineContext
- Created `OrchestrationIntegrationTests.cpp` with 10 integration tests

## Integration tests
1. EngineContext.RequestTransition delegates to ASM
2. EngineContext.RequestPush delegates to ASM
3. EngineContext.RequestPop delegates to ASM
4. EngineContext.ActivateOverlay delegates to OverlayStack
5. EngineContext.DeactivateOverlay delegates to OverlayStack
6. State transition triggers capability recompute on OverlayStack
7. IStateUI attaches on state enter and detaches on state exit
8. IStateUI suspends on push and resumes on pop
9. Lifecycle hooks fire during transitions through EngineContext
10. Full cascade: AppBuilder -> ASM + OverlayStack -> EngineContext

## Files changed
- `engine/wayfinder/src/app/EngineContext.h` (MODIFIED)
- `engine/wayfinder/src/app/EngineContext.cpp` (MODIFIED)
- `tests/app/OrchestrationIntegrationTests.cpp` (NEW)
- `tests/CMakeLists.txt` (MODIFIED)

## Test results
- All 4 test suites pass (core, render, scene, physics)
- 10 new integration tests all green
