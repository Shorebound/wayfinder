---
phase: 02
slug: subsystem-infrastructure
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-04-04
---

# Phase 02 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | doctest (thirdparty/doctest) |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | `cmake --build --preset debug --target wayfinder_core_tests && ctest --preset test -R SubsystemTests` |
| **Full suite command** | `cmake --build --preset debug && ctest --preset test` |
| **Estimated runtime** | ~15 seconds (quick), ~60 seconds (full) |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build --preset debug --target wayfinder_core_tests && ctest --preset test -R SubsystemTests`
- **After every plan wave:** Run `cmake --build --preset debug && ctest --preset test`
- **Before `/gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 60 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 02-01-01 | 01 | 1 | SUB-02 | unit | `ctest --preset test -R SubsystemTests` | ✅ (extend) | ⬜ pending |
| 02-01-02 | 01 | 1 | SUB-04 | unit | `ctest --preset test -R SubsystemTests` | ✅ (extend) | ⬜ pending |
| 02-01-03 | 01 | 1 | SUB-05 | unit | `ctest --preset test -R SubsystemTests` | ✅ (extend) | ⬜ pending |
| 02-02-01 | 02 | 1 | SUB-03, CAP-02, CAP-03, CAP-04, CAP-05 | unit | `ctest --preset test -R SubsystemTests` | ✅ (extend) | ⬜ pending |
| 02-03-01 | 03 | 2 | SUB-06 | unit | `ctest --preset test -R SubsystemTests` | ✅ (extend) | ⬜ pending |
| 02-03-02 | 03 | 2 | SUB-07 | unit | `ctest --preset test -R SubsystemTests` | ✅ (extend) | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] Extend `tests/app/SubsystemTests.cpp` with test cases for dependency ordering, cycle detection, capability gating, abstract-type resolution, Result propagation
- [ ] Add test cases for EngineContext v2 construction and subsystem access
- [ ] Add test cases for EngineContextRef flecs singleton pattern
