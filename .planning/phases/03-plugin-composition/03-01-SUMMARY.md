# Plan 03-01 Summary: Extract TopologicalSort & Create SubsystemManifest

## Outcome
Separated build-time registration from runtime ownership. SubsystemRegistry becomes build-only; SubsystemManifest owns instances at runtime.

## What Was Built

### TopologicalSort (engine/wayfinder/src/core/TopologicalSort.h)
- Standalone Kahn's algorithm utility extracted from SubsystemRegistry
- Reusable by AppBuilder and any future dependency graph resolution

### SubsystemManifest (engine/wayfinder/src/app/SubsystemManifest.h)
- Runtime container owning subsystem instances with Get/TryGet/Initialise/Shutdown
- Produced by SubsystemRegistry::Finalise()

### SubsystemRegistry Retrofit
- Finalise() now returns Result<SubsystemManifest<TBase>>
- Runtime methods removed from registry (build-time only)
- EngineContext holds SubsystemManifest* instead of SubsystemRegistry*

## Commits
- 93736a6: feat(03-01): extract TopologicalSort utility and create SubsystemManifest
- f7217a8: feat(03-01): retrofit SubsystemRegistry to build-time only with SubsystemManifest

## Files Changed
- Created: TopologicalSort.h, SubsystemManifest.h
- Modified: SubsystemRegistry.h, EngineContext.h/.cpp, CMakeLists.txt, SubsystemTests.cpp
