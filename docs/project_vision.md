# Project Vision

Wayfinder is a fantasy-console engine. The idea is simple: what if PS2/Dreamcast-era developers had modern hardware, architectural knowledge, and tooling, but kept the same artistic sensibility? Not chasing photorealism, not recreating hardware limitations, just building the kind of stylised, simulation-friendly games that era was known for, without the corners they had to cut.

## Creative Direction

Games built with Wayfinder should look like games on purpose. Art direction should be intentional, not whatever falls out of a PBR pipeline.

The engine leans into runtime systems: dynamic lighting, time-of-day, weather, physics. Things that were too expensive to do properly in 2001 but aren't anymore. Post-processing should reinforce mood, not simulate a camera.

## Principles

- **Dynamic over baked.** Prefer runtime computation over offline preprocessing. Lighting, simulation, environment state.
- **Expressive over realistic.** Art direction comes first. Use physically-based techniques when they help the look, ignore them when they don't.
- **Simple over ornate.** Clear data models, cheap iteration. Add complexity when it solves a real problem, not before.
- **Explicit over implicit.** Runtime behaviour comes from data and systems, not hidden engine magic.
- **Retro look, modern tools.** The target aesthetic is old. The build system, validation, iteration speed, and debugging shouldn't be.

## Non-Goals

- Photorealistic rendering
- Bake-heavy content pipelines
- General-purpose engine for every art style
- Physical accuracy over art direction
- Recreating historical workflow pain for nostalgia
