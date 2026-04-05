# Phase 6: Application Rewrite and Integration - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md -- this log preserves the alternatives considered.

**Date:** 2026-04-05
**Phase:** 06-application-rewrite-and-integration
**Areas discussed:** Frame loop rewrite strategy, Journey sandbox migration, Test migration approach, V1 code coexistence during Phase 6

---

## Platform Subsystem Collapse (emerged from discussion)

User identified that Phase 5 AppSubsystems are thin wrappers around PlatformBackend enum factory dispatch -- three tiers of indirection (abstract interface + enum factory + subsystem wrapper) when the plugin architecture should handle this naturally.

| Option | Description | Selected |
|--------|-------------|----------|
| Collapse now | Merge platform interface + subsystem into one type (SDLWindowSubsystem IS the subsystem). Remove PlatformBackend enum. Bigger scope but cleaner end state. | Y |
| Defer to future phase | Keep current wrappers. Phase 6 focuses on frame loop + Journey + tests. File issue for later. | |
| Partial collapse | Remove enum, let plugins register concrete subsystem directly, keep abstract Window as internal. | |

**User's choice:** Collapse now
**Notes:** User observed the subsystems are superfluous when they just wrap the platform backend enum. Wanted direct implementations like SDLInputSubsystem rather than InputSubsystem wrapping Input::Create(PlatformBackend::SDL3).

## SDL Platform Plugin Structure

| Option | Description | Selected |
|--------|-------------|----------|
| Group(3) + RenderDevice + Renderer | SDLPlatformPlugins = group of Window+Input+Time. SDLRenderDevicePlugin separate. Renderer plugin separate. | Y |
| Five individual plugins | All five as individual plugins, no group. Maximum flexibility, slightly more verbose. | |
| Group(5) single plugin | SDLPlatformPlugin = group of all 5. One call does everything. Less flexible but simplest. | |

**User's choice:** Group of 3 (Window+Input+Time) + separate RenderDevice + separate Renderer
**Notes:** User wanted explicit naming (SDLPlatformPlugins plural). Asked about build-time stripping -- confirmed runtime-only capability gating for Phase 6, file decomposition enables future CMake options. User asked about headless applications and how unused code could be excluded from builds -- explained the CMake module approach for a future phase.

## Build-Time Stripping

| Option | Description | Selected |
|--------|-------------|----------|
| Runtime only (Phase 6) | Runtime capability-gating only. Build-time stripping deferred. | Y |
| Add CMake options now | Add CMake options for SDL3/SDL_GPU. More work but removes dead code. | |

**User's choice:** Runtime only
**Notes:** User was exploring the concept, not requesting implementation. Confirmed the plugin decomposition makes this trivial to add later.

## Frame Loop Rewrite Strategy

| Option | Description | Selected |
|--------|-------------|----------|
| Atomic switchover | One plan: remove EngineRuntime, rewrite Application::Loop(), update Journey, update tests all at once. | Y |
| Incremental migration | Multiple plans: wire v2 alongside v1, switch Journey, migrate tests, remove v1. | |
| Feature flag | V2 loop behind flag/config. Both paths exist briefly. | |

**User's choice:** Atomic switchover
**Notes:** User explicitly stated: "We don't care about migration, we don't care about legacy. We don't want any leftover code. This project is greenfield, we have no developers. We just make the changes now and make it work."

## V2 Frame Sequence Order

| Option | Description | Selected |
|--------|-------------|----------|
| Architecture doc order | ProcessPending -> PollEvents -> Overlay events -> State events -> State update -> Overlay update -> State render -> Overlay render -> Present | Y |
| State-first events | ProcessPending -> PollEvents -> State events -> Overlay events -> State update -> Overlay update -> State render -> Overlay render | |

**User's choice:** Architecture doc order (overlays consume events first, top-down)
**Notes:** User initially asked for clarification on why overlays process events before states. Explanation: overlays are on top (debug console catches keypresses before game state). Update/render is bottom-up (state first, overlays on top). User confirmed this made sense.

## Journey Sandbox Migration

| Option | Description | Selected |
|--------|-------------|----------|
| Three plugins | SDLPlatformPlugins (5 subsystems) + EngineRenderPlugin + JourneyPlugin (game). | Y |
| Two plugins | SDLPlatformPlugin (subsystems + render) + JourneyPlugin (game). | |

**User's choice:** Three plugins (platform group, render, game)
**Notes:** User agreed Journey is just a sandbox -- simplest correct structure. JourneyPlugin registers GameplayState, components, systems, initial state.

## Test Migration Approach

| Option | Description | Selected |
|--------|-------------|----------|
| Full audit and rewrite | Review every test file. Rewrite any that test v1 patterns. | Y |
| Rewrite what breaks | Only fix tests that depend on removed v1 types. | |
| Minimal | Only fix compilation errors. | |

**User's choice:** Full audit and rewrite

## Application Integration Test

| Option | Description | Selected |
|--------|-------------|----------|
| Add Application integration test | Boots Application with Null plugins, enters state, ticks frames, validates canvases. | Y |
| Skip | Journey is the integration test. | |

**User's choice:** Add integration test

## V1 Code Removal Timing

| Option | Description | Selected |
|--------|-------------|----------|
| Remove in Phase 6 | Remove EngineRuntime, LayerStack, Game, old Plugin, FpsOverlayLayer during rewrite. | Y |
| Leave for Phase 7 | Phase 6 rewrites but leaves v1 types compilable. Phase 7 cleans up. | |

**User's choice:** Remove in Phase 6

## Phase 7 Scope

| Option | Description | Selected |
|--------|-------------|----------|
| Audit pass only | Phase 7 = lint/audit for dead includes and references. | Y |
| Split removal | Phase 6 big types, Phase 7 smaller types. | |
| Absorb into Phase 6 | Fold Phase 7 entirely into Phase 6. | |

**User's choice:** Phase 7 = audit pass only

---

## Null Implementation Removal

User questioned whether Null implementations are necessary in v2 architecture. Concluded: headless = don't add the plugin, no fake subsystems needed.

| Option | Description | Selected |
|--------|-------------|----------|
| Remove Null impls | Headless = no plugin registered. NullTime's deterministic tick moves to test infra as FixedTimeSubsystem. | Y |
| Keep as NullSubsystems | Convert to standalone AppSubsystems for tests. | |
| Keep NullTime only | Remove NullWindow/NullInput/NullDevice, keep NullTime as engine type. | |

**User's choice:** Remove Null impls, rename NullTime to FixedTimeSubsystem in test infrastructure
**Notes:** User observed Null types only exist because v1 factory pattern required a concrete subclass. V2 subsystem registry doesn't have that constraint. Agreed FixedTimeSubsystem is a better name for what the deterministic time source actually does.

---

## Agent's Discretion

- Internal layout of collapsed SDL subsystem implementations
- How Window close event maps to Application quit
- EngineRenderPlugin internal structure
- Whether abstract platform interfaces are kept as documentation or fully removed
- EventQueue integration with new frame loop
- JourneyPlugin::Build() implementation details
- NullRendererSubsystem implementation

## Deferred Ideas

- Build-time platform stripping (CMake options per backend)
- Alternative render backends (bgfx, wgpu)
- Platform module boundaries (wayfinder::core, wayfinder::sdl3_platform, etc.)
