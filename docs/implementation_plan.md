# Implementation Plan

## Purpose

This document describes the order in which Wayfinder should be built out from here.

The sequencing matters because the project is chasing a specific outcome: a stylized, dynamic engine that behaves like a modern runtime while aiming at a sixth-generation console aesthetic. That goal will fail if the project jumps into ambitious rendering features before the runtime, authoring model, and tooling boundaries are stable.

## Planning Rules

Wayfinder should keep following these priorities:

1. stabilize runtime and data boundaries before ambitious feature work
2. prefer explicit data models over hidden engine shortcuts
3. invest in headless validation before editor-only workflows
4. add visual complexity only after the renderer has a clean submission boundary
5. keep the engine modular where it protects iteration, not where it creates ceremony

## Current Baseline

The project already has a meaningful foundation:

- a CMake workspace split into engine, sandbox, tools, apps, and docs
- a working `wayfinder` engine library
- a working `journey` sandbox executable
- Flecs-owned scene state
- authored TOML scenes and prefabs
- scene validation and save support
- UUID-backed typed identifiers in runtime code
- headless validation commands through `waypoint`
- registered runtime systems for world transforms and active camera extraction
- an initial scene-to-frame extraction path for cameras, renderables, and debug cubes

The main gaps are no longer basic runtime bootstrapping. They are authoring maturity, render extraction, editor bring-up, and higher-level simulation and visual ambition.

## Roadmap

### Phase 1: Runtime Foundation

Goal: keep the runtime clean, explicit, and easy to extend.

Focus areas:

- preserve `Scene` as the world boundary
- keep components data-oriented
- expand scene-scoped systems through explicit module registration
- keep scene loading, validation, and save symmetry healthy
- prepare runtime data shapes for physics and future render extraction

Exit criteria:

- scene boot remains authored-data driven
- runtime-only derived state is handled by systems
- scene reload and mutation stay predictable

### Phase 2: Authoring And Asset Pipeline

Goal: make hand-authored data and headless validation first-class.

Focus areas:

- keep scene and prefab schemas narrow and explicit
- continue moving asset validation into `waypoint`
- strengthen asset registry behavior and asset-root validation
- support reliable save and roundtrip workflows
- add materials and related asset types only when their runtime consumers are ready

Exit criteria:

- authored scenes and prefabs validate cleanly without the sandbox window
- asset identity and references are deterministic
- save and load workflows are trustworthy enough to support editor work

### Phase 3: Render Extraction

Goal: separate scene state from render submission state.

Focus areas:

- finish replacing long-term direct ECS traversal inside `Renderer`
- keep extracted frame data authoritative for cameras, renderables, lights, and debug primitives
- make materials and renderable state explicit
- give `RenderPipeline` a real job once the frame boundary exists

Exit criteria:

- renderer consumes a stable engine-owned frame model
- scene traversal policy is no longer permanently embedded in renderer code
- future rendering features can be added without reshaping scene semantics every time

### Phase 4: Cartographer Bootstrap

Goal: build a practical first editor on top of stable runtime and authoring rules.

Focus areas:

- basic shell bring-up
- scene open and save
- hierarchy view
- inspector
- viewport
- entity creation, deletion, and transform editing

Exit criteria:

- Cartographer is useful for ordinary scene iteration
- scene semantics are still shared with runtime and tooling code rather than duplicated inside the editor

### Phase 5: Simulation And Runtime Systems

Goal: expand the world model after the data path and renderer boundary are stable.

Focus areas:

- integrate Jolt as the near-term physics path
- define ECS-facing physics bodies and sync rules
- add cloth and other simulation systems only through explicit runtime data and update models
- add diagnostics and inspection tools that help reason about live runtime state

Exit criteria:

- simulation systems compose with the existing scene and authoring model
- tooling can inspect and validate more than just file syntax

### Phase 6: Visual Ambition

Goal: deliver the retro-modern visual thesis in layers.

Focus areas:

- stylized material and shading models
- dynamic lighting workflows
- time of day
- atmosphere and volumetrics
- indirect lighting or similar dynamic global illumination approximations
- fixed internal resolution and upscale controls where that strengthens the look

Exit criteria:

- the renderer supports expressive, dynamic worlds without turning into a realism-first pipeline
- visual systems reinforce the project thesis instead of fighting it

## What Not To Rush

Wayfinder should avoid the following sequencing mistakes:

- building a large editor before the data model is trustworthy
- overdesigning a render graph before there is stable frame extraction
- adding many new asset schemas before validation and save support exist for them
- treating placeholder applications and tools as if they were active products
- chasing physically exact realism before the stylized rendering model is coherent

## Immediate Next Focus

From the current baseline, the most valuable next work is:

1. strengthen the authoring and validation path around scenes, prefabs, and future materials
2. finish hardening the extracted frame boundary so materials, lighting, and editor overlays use it consistently
3. bring Cartographer online only after those two layers are reliable
