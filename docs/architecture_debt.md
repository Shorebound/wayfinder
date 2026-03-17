# Architecture Debt

## Purpose

This document ranks the most important cleanup work to finish before Cartographer grows beyond a basic shell.

The goal is not purity. The goal is to keep the runtime, authoring model, and renderer from accumulating shortcuts that would force the editor to compensate for them later.

## Priority 1: Extracted Frame Data

Current status:

- `Renderer` still performs direct ECS traversal for renderables and debug lights
- active camera state is already extracted, but renderables and lights are not yet in an engine-owned frame model

Why it matters:

- editor viewport behavior will become harder to reason about if scene traversal policy keeps living inside renderer code
- render features, visibility rules, and editor overlays will stay coupled to Flecs query details longer than they should

Recommended next move:

- define extracted frame structs for camera, renderables, and debug primitives
- move ECS traversal into a scene-to-frame extraction pass
- keep `Renderer` focused on consuming frame data instead of discovering scene meaning

## Priority 2: Service Locator Scope

Current status:

- platform and rendering services are still accessed through `ServiceLocator`
- this is acceptable for low-level systems today, but it is still a global dependency surface

Why it matters:

- editor tools, runtime diagnostics, and future simulation systems should not quietly grow around global service access
- implicit dependencies make it harder to test subsystem behavior outside the full app shell

Recommended next move:

- keep `ServiceLocator` limited to low-level platform services only
- prefer explicit constructor or method dependencies for scene extraction, editor state, and future gameplay systems

## Priority 3: Backend Selection Boundaries

Current status:

- `Application` now uses abstract window creation instead of directly naming the Raylib window backend
- backend selection still lives in backend implementation files and compile-time wiring rather than a fuller runtime configuration layer

Why it matters:

- the engine is modular enough for the current stage, but backend evolution will be cleaner if the selection boundary stays centralized and explicit

Recommended next move:

- keep creation behind abstract factories
- introduce a small renderer/platform backend configuration object before adding alternate backends

## Priority 4: Asset Pipeline Depth

Current status:

- asset scanning now distinguishes prefab and material assets
- the material path is still intentionally simple and Raylib-oriented

Why it matters:

- Cartographer needs stable asset identity and validation rules, not a half-dozen special-case file readers

Recommended next move:

- add explicit asset categories only when they have runtime consumers and headless validation
- keep material assets narrow until extracted frame data exists

## What To Avoid

- adding large editor workflows that invent scene semantics outside runtime code
- adding many more asset types before validation and save symmetry exist for them
- treating renderer queries as the permanent place where scene meaning is decided