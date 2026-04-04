---
status: testing
phase: 03-plugin-composition
source: [03-01-SUMMARY.md, 03-02-SUMMARY.md, 03-03-SUMMARY.md, 03-04-SUMMARY.md, 03-05-SUMMARY.md]
started: 2026-04-05T00:00:00Z
updated: 2026-04-05T00:00:00Z
---

## Current Test
<!-- OVERWRITE each test - shows where we are -->

number: 1
name: Cold Start Build
expected: |
  Debug build completes with zero errors and zero warnings on the engine, tests, and sandbox targets.
awaiting: user response

## Tests

### 1. Cold Start Build
expected: Debug build compiles all targets (wayfinder, journey, tests) with zero errors and zero warnings.
result: pass

### 2. All Tests Pass
expected: ctest --preset test runs all 379 tests and reports 0 failures.
result: [pending]

### 3. Journey Sandbox Boots and Shuts Down
expected: Running journey.exe starts the application, initialises subsystems (log output shows subsystem init), renders at least one frame, and exits cleanly on close with no crashes or assertion failures.
result: [pending]

### 4. Plugin Composition Round-Trip
expected: A plugin added via Application::AddPlugin<T>() goes through Build() in dependency order. AppBuilder::Finalise() produces an AppDescriptor. Lifecycle hooks (OnAppReady, OnShutdown) fire at the correct points. AppDescriptor is accessible via EngineContext. (Verified via test output)
result: [pending]

### 5. Config 3-Tier TOML Layering
expected: AppBuilder::LoadConfig<T>("key") loads struct defaults (layer 1), merges project config/key.toml (layer 2), then merges saved/config/key.toml overrides (layer 3). Missing files fall back gracefully. Multiple calls with the same key parse TOML only once (caching). (Verified via test output)
result: [pending]

### 6. SubsystemManifest Separation
expected: SubsystemRegistry is build-time only. SubsystemRegistry::Finalise() returns Result<SubsystemManifest>. Runtime Get<T>/TryGet<T> accessors are on SubsystemManifest, not SubsystemRegistry. EngineContext holds SubsystemManifest. (Verified via test output and code inspection)
result: [pending]

## Summary

total: 6
passed: 0
issues: 0
pending: 6
skipped: 0

## Gaps

[none yet]
