---
phase: 04-orchestration
plan: 01
title: "Orchestration Vocabulary Types + IOverlay::OnEvent Signature Change"
subsystem: app
tags: [orchestration, types, interfaces]
requires: [Phase 3 complete]
provides: [OrchestrationTypes.h, extended IApplicationState, bool-returning IOverlay::OnEvent]
affects: [ApplicationStateMachine (04-02), OverlayStack (04-03), Simulation (04-04)]
tech-stack:
  added: []
  patterns: [designated-initialisers, constexpr-computation, variant-dispatch]
key-files:
  created:
    - engine/wayfinder/src/app/OrchestrationTypes.h
  modified:
    - engine/wayfinder/src/app/IApplicationState.h
    - engine/wayfinder/src/app/IOverlay.h
    - engine/wayfinder/CMakeLists.txt
    - tests/app/ApplicationStateTests.cpp
key-decisions:
  - "Conservative defaults for BackgroundPreferences (no update, no render) and SuspensionPolicy (no update, yes render)"
  - "PendingOperation uses std::variant with monostate for empty state"
  - "IOverlay::OnEvent returns bool (false = pass through) for event consumption in OverlayStack"
requirements-completed: [STATE-05]
duration: "8 min"
completed: "2026-04-06"
---

# Phase 04 Plan 01: Orchestration Vocabulary Types Summary

Orchestration vocabulary types (BackgroundPreferences, SuspensionPolicy, PendingOperation variant, registration descriptors) and IOverlay::OnEvent bool-return breaking change - all Wave 2 systems build on these types.

## Tasks Completed

| # | Task | Files | Commit |
|---|------|-------|--------|
| 1 | Define orchestration vocabulary types + extend IApplicationState | 4 files | 9254c30 |
| 2 | IOverlay::OnEvent signature change + mock updates | 2 files | 9254c30 |

## What Was Built

- **OrchestrationTypes.h**: BackgroundPreferences, SuspensionPolicy, EffectiveBackgroundPolicy structs with constexpr ComputeBackgroundPolicy AND intersection. PendingOperation variant (monostate/FlatTransition/PushTransition/PopTransition). StateRegistrationDescriptor and OverlayDescriptor.
- **IApplicationState extended**: GetBackgroundPreferences() and GetSuspensionPolicy() virtual methods with conservative defaults
- **IOverlay::OnEvent**: Changed from void to bool return (false = pass through), enabling event consumption in OverlayStack
- **MockOverlay updated**: ConsumeEvents flag for testing event consumption

## Deviations from Plan

None - plan executed exactly as written.

## Verification

- All 4 test suites pass (render, core, scene, physics)
- OrchestrationTypes.h compiles with all required types
- IApplicationState has negotiation virtual methods
- IOverlay::OnEvent returns bool with false default
- Pre-commit hook auto-formatted OrchestrationTypes.h (clang-format)

## Issues Encountered

None.

## Next Phase Readiness

Ready for Wave 2: Plans 04-02 (ApplicationStateMachine), 04-03 (OverlayStack), and 04-04 (Simulation) can proceed in parallel - all depend only on 04-01 outputs.
