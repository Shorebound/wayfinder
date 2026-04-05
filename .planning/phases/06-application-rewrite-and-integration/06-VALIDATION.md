---
phase: 06
slug: application-rewrite-and-integration
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-04-05
---

# Phase 06 - Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | doctest |
| **Config file** | tests/CMakeLists.txt |
| **Quick run command** | `ctest --preset test -R "core_tests"` |
| **Full suite command** | `ctest --preset test` |
| **Estimated runtime** | ~30 seconds |

---

## Sampling Rate

- **After every task commit:** `cmake --build --preset debug && ctest --preset test -R "core_tests"`
- **After every plan wave:** `cmake --build --preset debug && ctest --preset test`
- **Before `/gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 30 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 06-01-01 | 01 | 1 | APP-02 | integration | `ctest --preset test -R "core_tests"` | No - W0 | pending |
| 06-02-01 | 02 | 1 | APP-02 | unit | `cmake --build --preset debug` | Yes (existing) | pending |
| 06-03-01 | 03 | 2 | APP-03 | build | `cmake --build --preset debug --target journey` | Yes (existing) | pending |
| 06-04-01 | 04 | 3 | APP-04 | unit | `ctest --preset test` | Yes (existing) | pending |

*Status: pending / green / red / flaky*

---

## Wave 0 Requirements

- [ ] `tests/app/ApplicationIntegrationTests.cpp` - headless frame sequence validation (APP-02, D-14)
- [ ] `tests/FixedTimeSubsystem.h` - deterministic time subsystem for test infrastructure (D-02a)

*Existing test infrastructure (doctest, TestHelpers.h) covers all other phase requirements.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Journey sandbox renders frames | APP-03 | Requires GPU + window | Build journey target, run, observe rendering output |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 30s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
