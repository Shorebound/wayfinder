---
status: testing
phase: 01-foundation-types
source:
  - 01-01-SUMMARY.md
  - 01-02-SUMMARY.md
  - 01-03-SUMMARY.md
  - 01-04-SUMMARY.md
started: 2026-04-04T18:00:00Z
updated: 2026-04-04T18:00:00Z
---

## Current Test
<!-- OVERWRITE each test - shows where we are -->

number: 1
name: Full Build and Test Suite
expected: |
  `cmake --build --preset debug` compiles all targets with 0 errors.
  `ctest --preset test` passes all 4 suites (render, core, scene, physics) with 0 failures.
awaiting: user response

## Tests

### 1. Full Build and Test Suite
expected: All targets compile (0 errors), all 4 test suites pass (0 failures).
result: [pending]

### 2. Tag System Rename Complete
expected: No references to "GameplayTag", "GameplayTagContainer", or "GameplayTagRegistry" remain in engine source or test code (excluding data-file JSON key "gameplay_tags" kept for backward compat). Grepping for "GameplayTag" in engine/wayfinder/src/ and tests/ returns 0 hits (except ComponentRegistry's JSON key).
result: [pending]

### 3. Tag Construction Restricted to Registries
expected: Tag.h has private constructors with `friend class TagRegistry` and `friend class Plugins::PluginRegistry`. No public FromName or FromInterned methods exist on Tag. Tests construct tags exclusively through TagRegistry.
result: [pending]

### 4. DLL Plugin System Fully Removed
expected: The files PluginExport.h, PluginLoader.h, PluginLoader.cpp, and EntryPoint.h do not exist under engine/wayfinder/src/plugins/. No CreateGamePlugin declaration exists in Plugin.h. No journey_plugin shared library target exists in sandbox/journey/CMakeLists.txt.
result: [pending]

### 5. Sandbox Apps Use Explicit main()
expected: sandbox/journey/src/JourneyGame.cpp and sandbox/waystone/src/WaystoneApplication.cpp each contain an explicit `int main()` entry point (not a macro-based entry). Journey executable launches without crash (exit code 0 or user-closed).
result: [pending]

### 6. V2 Interface Headers Exist
expected: All 8 v2 headers exist: IApplicationState.h, IOverlay.h, AppSubsystem.h, StateSubsystem.h (in app/), IPlugin.h, IStateUI.h, ServiceProvider.h (in plugins/), Capability.h (in gameplay/). Each is listed in engine/wayfinder/CMakeLists.txt.
result: [pending]

### 7. StateMachine Template Works with Multiple Key Types
expected: StateMachineTests.cpp contains tests using both InternedString and enum class as state IDs. All 12 StateMachine tests pass (visible in wayfinder_core_tests output).
result: [pending]

## Summary

total: 7
passed: 0
issues: 0
pending: 7
skipped: 0

## Gaps

[none yet]
