# Plan 03-05 Summary: Application AddPlugin + EngineContext AppDescriptor

## Outcome
Completed the runtime bridge connecting plugin composition to the engine lifecycle.

## What Was Built

### Application::AddPlugin<T>()
- Public API on Application for adding plugins before Initialise()
- Lazy-creates an internal AppBuilder on first call
- Delegates to AppBuilder::AddPlugin<T>()

### Application::Initialise() v2 Builder Finalisation
- After config loading and before plugin registration, finalises the builder
- Sets project paths from ProjectDescriptor, calls Finalise(), stores AppDescriptor
- Propagates AppDescriptor to EngineContext for subsystem access
- Entirely skipped when no builder exists (v1 path backward compatibility)

### EngineContext AppDescriptor Access
- `GetAppDescriptor() -> const AppDescriptor&` (asserts if not set)
- `TryGetAppDescriptor() -> const AppDescriptor*` (safe null check)
- `SetAppDescriptor(const AppDescriptor* descriptor)` (non-owning pointer)

## Commits
- d5d5fb9: feat(03-05): Application::AddPlugin, EngineContext AppDescriptor access

## Tests Added
- 5 new test cases in ApplicationStateTests.cpp
- EngineContext AppDescriptor Access: null default, SetAppDescriptor wiring, TryGetAppDescriptor pointer
- AppBuilder Integration: AddPlugin -> Finalise -> AppDescriptor round-trip with lifecycle hooks firing, AppDescriptor accessible via EngineContext

## Test Count
379 tests (all passing)
