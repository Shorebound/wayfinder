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

Runtime-facing scene data now uses engine-owned math and render value types rather than exposing Raylib vector and matrix types through scene components.

## Key Boundaries

### Application

`Application` should stay thin.

It is responsible for:

- process startup and shutdown
- window creation
- low-level service initialization
- selecting the current platform and rendering backends through the bootstrap configuration
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

Scene loading now relies on a shared asset service for prefab and material resolution so validation and runtime preparation do not silently diverge around separate asset-registry scans.

### Entities And Components

`Entity` is a lightweight handle over Flecs state. Components should stay narrow and data-oriented.

The current runtime already works with explicit scene metadata, transform, mesh, camera, light, and prefab-reference style data. Hierarchy authority lives in Flecs `ChildOf` relationships, and runtime-only derived state such as world transforms and active camera selection is kept separate from authorable component data.

Renderable intent is now also explicit instead of being decided only by "entity has a mesh" rules. Scene data can describe visibility, scene layer, and sort priority through the authorable `renderable` component, and extraction carries that intent forward into `RenderFrame` submissions.

### Runtime Modules

Scene-scoped runtime logic should be registered explicitly as modules and systems.

That is already true for:

- world-transform propagation through `WorldTransformComponent`
- active camera extraction through `ActiveCameraStateComponent`

This keeps derivation logic out of ad hoc renderer code and makes scene startup more predictable.

### Renderer

The renderer currently consumes engine-owned frame data after Flecs systems have run.

Today that frame boundary looks like this:

- scene modules derive world transforms and active camera state
- `SceneRenderExtractor` traverses the scene world and builds a `RenderFrame`
- `Renderer` prepares the frame and hands it to `RenderPipeline`
- `RenderPipeline` executes the extracted pass schedule against the active backend

That is the right direction for the project. The renderer no longer decides scene meaning by traversing Flecs directly.

The current extracted frame now carries explicit views, explicit passes, pass-local scene submissions, and pass-local debug payloads. Scene renderability is also explicit at the authoring boundary instead of being inferred only from `MeshComponent` presence.

What is still incomplete is backend reach, not the basic frame boundary. The current pipeline remains intentionally narrow and raylib-oriented. It supports a small explicit pass schedule rather than a broad render-graph style system, and the active Raylib adapter still exposes honest limits such as single-view execution, no render targets, and primitive-only geometry.

### Services

The service layer currently exposes platform-facing systems such as input, time, graphics context, and render API access.

This is acceptable as a short-term convenience boundary. It should not become the default dependency mechanism for gameplay rules, editor state, or authoring workflows.

Renderer execution now consumes explicit `IGraphicsContext` and `IRenderAPI` dependencies that are wired during application bootstrap instead of reaching back into `ServiceLocator` during every frame.

Asset lookup is no longer duplicated inside the renderer. Scene loading and renderer preparation now consume a shared runtime asset service owned above the scene and renderer boundaries.

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
9. `SceneRenderExtractor` builds a renderer-facing `RenderFrame` from the resulting scene state.
10. `Renderer` submits that frame through the current render pipeline and backend.

## Current Frame Flow

For each frame, the runtime is effectively doing this:

1. advance time
2. update the active scene
3. progress Flecs systems
4. extract a renderer-facing `RenderFrame`
5. prepare asset-backed materials for the extracted pass payloads
6. execute the extracted render passes for that frame

That gives Wayfinder a clean enough baseline for current work even though the frame model and pass system are still intentionally small.

## What Is Stable Today

The following architectural decisions are stable enough to build on:

- Flecs owns scene entity storage
- scene boot comes from authored TOML instead of hardcoded sample objects
- scene load validates before replacing active runtime state
- save and load flow through the same component registration model
- runtime-only derived data is produced by registered systems instead of being recomputed ad hoc in the renderer

## What Is Still Transitional

These areas are real but intentionally unfinished:

- the current Raylib-backed platform and rendering implementations
- service locator usage as the main cross-cutting convenience layer

## Current Raylib Limits

The checked-in renderer should now be read as a constrained Raylib adapter, not as the engine's ideal long-term rendering model.

Current backend limits are explicit:

- one active view at a time
- no render-target support
- primitive box geometry only
- debug pass support is limited to the currently mapped grid, line, and box helpers

The checked-in project also includes a headless `Null` rendering backend used to exercise the capability surface and render tests without depending on a real drawing context.

If future work needs more than that, the engine should extend the backend capability surface first instead of quietly teaching the renderer to assume those features exist.

## Architectural Rules

These rules matter because they protect the future direction of the engine:

- scene semantics must not live only in editor code
- authoring validation must exist outside the runtime window
- components should remain data-oriented
- runtime-derived state should be produced by systems, not hidden renderer logic
- rendering should keep extracted frame data as the only renderer-facing scene boundary
- renderer/backend limits should be stated explicitly in capability data rather than hidden in adapter implementation details
- new subsystems should justify themselves with clear runtime or workflow value

## Near-Term Direction

The next architectural improvement is not a giant rendering feature. It is a better frame boundary.

Near-term work should focus on:

- preserving the renderer-facing frame data as the authoritative boundary
- keeping backend capability checks explicit as new render features appear
- integrating future simulation systems through ECS components and modules instead of special-case runtime code
- preserving headless validation, tests, and save workflows as first-class behavior