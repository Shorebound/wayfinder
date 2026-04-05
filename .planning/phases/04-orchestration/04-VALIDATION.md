---
phase: 04
slug: orchestration
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-04-06
---

# Phase 04 - Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | doctest (via `doctest::doctest_with_main`) |
| **Config file** | None (header-only, linked via CMake) |
| **Quick run command** | `cmake --build --preset debug --target wayfinder_core_tests && ctest --preset test -R core` |
| **Full suite command** | `ctest --preset test` |
| **Estimated runtime** | ~10 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build --preset debug --target wayfinder_core_tests && ctest --preset test -R core`
- **After every plan wave:** Run `ctest --preset test`
- **Before `/gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 15 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 04-01-01 | 01 | 1 | STATE-05 | unit (compile) | `cmake --build --preset debug --target wayfinder_core_tests` | No - Wave 0 | pending |
| 04-01-02 | 01 | 1 | STATE-05 | unit | `wayfinder_core_tests -tc="IOverlay*"` | Yes (update) | pending |
| 04-02-01 | 02 | 2 | STATE-02, STATE-03, STATE-04, STATE-08 | unit | `wayfinder_core_tests -tc="ApplicationStateMachine*"` | No - Wave 0 | pending |
| 04-02-02 | 02 | 2 | STATE-02, STATE-08 | unit | `wayfinder_core_tests -tc="ApplicationStateMachine*push*"` | No - Wave 0 | pending |
| 04-03-01 | 03 | 2 | OVER-02, OVER-03 | unit | `wayfinder_core_tests -tc="OverlayStack*"` | No - Wave 0 | pending |
| 04-03-02 | 03 | 2 | OVER-04 | unit | `wayfinder_core_tests -tc="OverlayStack*toggle*"` | No - Wave 0 | pending |
| 04-04-01 | 04 | 2 | SIM-01, SIM-03 | unit | `wayfinder_core_tests -tc="Simulation*"` | No - Wave 0 | pending |
| 04-04-02 | 04 | 2 | SIM-06, SIM-07 | unit | `wayfinder_core_tests -tc="Simulation*ActiveGameState*"` | No - Wave 0 | pending |
| 04-05-01 | 05 | 3 | UI-02 | unit | `wayfinder_core_tests -tc="AppBuilder*State*"` | No - Wave 0 | pending |
| 04-05-02 | 05 | 3 | UI-02 | unit | `wayfinder_core_tests -tc="AppBuilder*Overlay*"` | No - Wave 0 | pending |
| 04-06-01 | 06 | 4 | UI-03 | unit | `wayfinder_core_tests -tc="*EngineContext*transition*"` | No - Wave 0 | pending |
| 04-06-02 | 06 | 4 | UI-03 | integration | `wayfinder_core_tests -tc="*integration*orchestration*"` | No - Wave 0 | pending |

*Status: pending / green / red / flaky*

---

## Wave 0 Requirements

- [ ] `tests/app/ApplicationStateMachineTests.cpp` - covers STATE-02, STATE-03, STATE-04, STATE-05, STATE-08
- [ ] `tests/app/OverlayStackTests.cpp` - covers OVER-02, OVER-03, OVER-04
- [ ] `tests/gameplay/SimulationTests.cpp` - covers SIM-01, SIM-03, SIM-07
- [ ] Update `tests/CMakeLists.txt` to add new test files to `wayfinder_core_tests`
- [ ] Update mock classes in `tests/app/ApplicationStateTests.cpp` for IOverlay::OnEvent return type change

*Note: Test files created inline within each plan's tasks. No separate Wave 0 plan needed.*

---

## Manual-Only Verifications

*All phase behaviors have automated verification.*

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 15s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
