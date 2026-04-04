---
phase: 01-foundation-types
plan: 03
subsystem: gameplay
tags: [state-machine, generic-template, graph-validation, deferred-transitions]

requires: []
provides:
  - "StateMachine<TStateId> generic template with descriptor-based registration"
  - "Graph validation at Finalise time (dangling targets, unreachable states)"
  - "Deferred transition model (TransitionTo + ProcessPending)"
  - "Transition observers firing after OnEnter"
affects: [game-framework, application-state-machine, sub-state-machines]

tech-stack:
  added: []
  patterns: [descriptor-based-registration, graph-validation-at-finalise, deferred-transitions, last-write-wins-pending]

key-files:
  created:
    - engine/wayfinder/src/gameplay/StateMachine.h
    - tests/gameplay/StateMachineTests.cpp
  modified:
    - engine/wayfinder/CMakeLists.txt
    - tests/CMakeLists.txt

key-decisions:
  - "Used second template parameter THash (default std::hash<TStateId>) instead of requiring InternedString::Hash member - more generic"
  - "assert() for invalid TransitionTo rather than Result<> - programmer error, not recoverable failure"
  - "Last-write-wins for multiple pending transitions (no queue) - simpler, matches typical game loop usage"

patterns-established:
  - "Descriptor-based registration: AddState takes aggregate descriptors, Finalise validates the graph"
  - "Deferred transition: TransitionTo queues, ProcessPending executes with correct callback ordering"

requirements-completed: [SIM-05]

duration: 8min
completed: 2026-04-04
---

# Phase 01 Plan 03: StateMachine<TStateId> Summary

**Generic state machine template with descriptor-based registration, BFS graph validation, deferred transitions, and transition observers - works with both InternedString and enum class key types.**

## Performance

- **Duration:** ~8 min
- **Started:** 2026-04-04T05:30:28Z
- **Completed:** 2026-04-04T05:39:00Z
- **Tasks:** 1 (TDD: RED + GREEN)
- **Files modified:** 4

## Accomplishments

- Header-only `StateMachine<TStateId, THash>` template with `requires std::equality_comparable<TStateId>` constraint
- `Finalise()` validates initial state exists, no dangling transition targets, and all states reachable via BFS - returns `Result<void>` with descriptive errors
- Deferred transition model: `TransitionTo` queues, `ProcessPending` fires OnExit(old) then OnEnter(new) then observers
- 12 comprehensive test cases covering validation edge cases, lifecycle, callback ordering, observer timing, last-write-wins, InternedString and enum class key types

## Task Commits

Each task was committed atomically (TDD flow):

1. **Task 1 RED: StateMachine tests + stub header** - `d958c79` (test)
2. **Task 1 GREEN: Full StateMachine implementation** - `76902da` (feat)

## Files Created/Modified

- `engine/wayfinder/src/gameplay/StateMachine.h` - Header-only generic state machine template
- `tests/gameplay/StateMachineTests.cpp` - 12 test cases covering all validation, lifecycle, and transition scenarios
- `engine/wayfinder/CMakeLists.txt` - Added StateMachine.h to Gameplay section
- `tests/CMakeLists.txt` - Added StateMachineTests.cpp to wayfinder_core_tests

## Decisions Made

- **THash as second template parameter:** Defaults to `std::hash<TStateId>`, allowing InternedString (which already has std::hash specialisation) and enum class (with custom hash) without requiring InternedString-specific infrastructure.
- **assert() for invalid transitions:** `TransitionTo` with a target not in `AllowedTransitions` is a programmer error, not a recoverable runtime failure. assert catches misuse in debug builds without adding Result overhead to the hot path.
- **Last-write-wins pending:** Calling `TransitionTo` twice before `ProcessPending` replaces the pending transition rather than queueing. This matches typical game loop patterns where you decide the transition in update and apply in a fixed phase.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- StateMachine<TStateId> is ready for use by ApplicationStateMachine (Phase 4) and per-state sub-state machines
- No blockers or concerns

## Self-Check: PASSED

- [x] `engine/wayfinder/src/gameplay/StateMachine.h` exists
- [x] `tests/gameplay/StateMachineTests.cpp` exists
- [x] Commit `d958c79` exists (RED)
- [x] Commit `76902da` exists (GREEN)
- [x] All 12 tests pass
- [x] Full test suite passes (4/4 suites, 0 failures)

---
*Phase: 01-foundation-types*
*Completed: 2026-04-04*
