# Runtime Architecture

## Purpose

This document explains how the runtime is currently structured, which boundaries matter, and where the architecture is intentionally still incomplete.

Wayfinder is not trying to preserve an abstract architecture diagram. It is trying to preserve clean ownership so the engine can grow toward more dynamic rendering and simulation without collapsing into a single monolithic loop.

## Runtime Overview

At a high level, the runtime is organized like this:

1. `Application` owns process lifetime, the window, and the main loop.
2. `Game` owns high-level runtime flow and the active `Scene`.
3. `Scene` owns the Flecs world and scene-scoped runtime state.
4. Runtime modules register systems that derive or maintain scene state.
5. `Renderer` reads scene state and submits draw calls through the current render backend.

Today, the active backend is raylib and the active ECS is Flecs.

## Key Boundaries

### Application

`Application` should stay thin.

It is responsible for:

- process startup and shutdown
- window creation
- low-level service initialization
- running the main loop
- creating the `Game` and `Renderer`

It should not become the place where scene semantics, content loading rules, or rendering policy are invented.

### Game

`Game` owns the active scene and high-level runtime progression.

It is the bridge between the outer application loop and the scene runtime. It decides what scene is active and when scene-level update work happens.

### Scene

`Scene` is the runtime world boundary.

It is responsible for:

- owning the Flecs world
- initializing component and module registration
- instantiating runtime scene state from validated scene documents
- creating entities from validated component data
- clearing and replacing scene contents safely

It should remain the place where scene data becomes runtime state. Authored file parsing and serialization should stay outside the runtime `Scene` object so the runtime boundary does not turn into a giant parser switchboard.

### Entities And Components

`Entity` is a lightweight handle over Flecs state. Components should stay narrow and data-oriented.

The current runtime already works with explicit scene metadata, transform, mesh, camera, light, and prefab-reference style data. Hierarchy authority lives in Flecs `ChildOf` relationships, and runtime-only derived state such as world transforms and active camera selection is kept separate from authorable component data.

### Runtime Modules

Scene-scoped runtime logic should be registered explicitly as modules and systems.

That is already true for:

- world-transform propagation through `WorldTransformComponent`
- active camera extraction through `ActiveCameraStateComponent`

This keeps derivation logic out of ad hoc renderer code and makes scene startup more predictable.

### Renderer

The renderer currently consumes scene state after Flecs systems have run. It still performs direct ECS queries for camera, mesh, and light data.

That is acceptable for the current stage of the project, but it is not the intended long-term shape. The renderer should move toward consuming extracted frame data owned by the engine rather than permanently owning ECS traversal policy.

### Services

The service layer currently exposes platform-facing systems such as input, time, graphics context, and render API access.

This is acceptable as a short-term convenience boundary. It should not become the default dependency mechanism for gameplay rules, editor state, or authoring workflows.

## Current Boot Flow

The runtime path in the checked-in project works like this:

1. `journey` provides the concrete `Application` creation entry point.
2. `Application` initializes logging and low-level services, creates the window, and constructs `Game` and `Renderer`.
3. `Game` resolves a bootstrap scene path.
4. `Scene::Initialize()` registers core authorable components and core runtime modules.
5. `Scene::LoadFromFile(...)` loads a validated scene document before mutating the active runtime state.
6. Prefab data is resolved and merged before entity-local overrides are applied.
7. Entities are created in the Flecs world, and hierarchy relationships are established.
8. Runtime systems derive world transforms and active camera state during scene update.
9. The renderer reads the resulting scene state and submits the current frame.

## Current Frame Flow

For each frame, the runtime is effectively doing this:

1. advance time
2. update the active scene
3. progress Flecs systems
4. read derived camera and transform state
5. render visible scene content

That gives Wayfinder a clean enough baseline for current work even though frame extraction is still ahead.

## What Is Stable Today

The following architectural decisions are stable enough to build on:

- Flecs owns scene entity storage
- scene boot comes from authored TOML instead of hardcoded sample objects
- scene load validates before replacing active runtime state
- save and load flow through the same component registration model
- runtime-only derived data is produced by registered systems instead of being recomputed ad hoc in the renderer

## What Is Still Transitional

These areas are real but intentionally unfinished:

- direct ECS traversal inside `Renderer`
- the stubbed `RenderPipeline`
- the current raylib-shaped rendering internals
- service locator usage as the main cross-cutting convenience layer

## Architectural Rules

These rules matter because they protect the future direction of the engine:

- scene semantics must not live only in editor code
- authoring validation must exist outside the runtime window
- components should remain data-oriented
- runtime-derived state should be produced by systems, not hidden renderer logic
- rendering should move toward extracted frame data before large rendering ambition is added
- new subsystems should justify themselves with clear runtime or workflow value

## Near-Term Direction

The next architectural improvement is not a giant rendering feature. It is a better frame boundary.

Near-term work should focus on:

- extracting renderer-facing frame data from the scene
- keeping renderables and materials explicit
- integrating future simulation systems through ECS components and modules instead of special-case runtime code
- preserving headless validation and save workflows as first-class behavior