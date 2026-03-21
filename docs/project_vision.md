# Project Vision

## Premise

Wayfinder is a fantasy-console engine.

The thought experiment is straightforward: imagine a 2001-era console development culture with 2026-era compute budgets, architecture knowledge, and tooling convenience. The artistic target still belongs to the sixth console generation. The production values do not suddenly become film realism. What changes is how much of the world can stay alive, dynamic, and responsive at runtime.

This project is not trying to simulate hardware constraints from that era. It is trying to preserve the sensibility of that era while removing the reasons developers had to fake everything.

## Creative Target

Wayfinder should feel at home with:

- bold silhouettes
- stylised materials
- readable lighting
- simple but expressive geometry
- dramatic atmosphere
- strong time-of-day and weather changes
- worlds that react at runtime instead of relying on baked tricks

The engine should be comfortable building games that look game-like on purpose.

## Core Principles

### Dynamic Over Baked

If something can be computed robustly at runtime, that should be the default bias. This applies to lighting, environment response, simulation, and scene state.

### Expressive Over Realistic

Physical correctness is useful when it supports the look. It is not the goal by itself. The project should prioritize clarity, mood, stylisation, and readability before realism.

### Simple Over Ornate

Wayfinder should prefer clear data models, understandable systems, and cheap iteration. Complex technology is acceptable only when it buys a clear creative or workflow advantage.

### Stable Runtime Semantics

Runtime behaviour should come from explicit data and systems rather than from hidden engine shortcuts. Scenes, prefabs, simulation, and rendering all benefit from predictable rules.

### Modern Workflow, Retro Taste

The target look is retro-inspired. The development experience should not be. Validation, iteration, debugging, and tooling should feel contemporary.

## What This Means In Practice

Wayfinder should eventually support systems such as:

- dynamic lights
- stable real-time indirect lighting or a similarly dynamic approximation
- time of day
- volumetrics and atmosphere
- physics and cloth simulation
- runtime-friendly materials
- expressive post-processing that reinforces the visual thesis rather than chasing realism

Those systems should serve stylised worlds first. They do not need to mimic modern blockbuster rendering stacks to be valuable.

## Non-Goals

Wayfinder is not trying to become:

- a photorealistic renderer
- a bake-heavy content pipeline
- a general-purpose engine that optimizes for every art style equally
- a perfect simulation stack where physical accuracy overrides art direction
- a nostalgia project that recreates historical workflow pain

## Why The Architecture Matters

The project vision only works if the engine stays honest about its boundaries.

If authoring rules live only inside an editor prototype, the runtime becomes brittle. If rendering policy is trapped inside ad hoc scene traversal, dynamic systems become harder to evolve. If assets cannot be validated outside the main app, iteration slows down. The technical structure exists to protect the creative goal.

That is why the current work focuses on runtime boundaries, authored data, and headless validation before larger editor or rendering ambitions.