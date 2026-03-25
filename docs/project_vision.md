# Project Vision

Wayfinder is a fantasy-console engine. The idea is simple: what if PS2/Dreamcast-era developers had modern hardware, architectural knowledge, and tooling, but kept the same artistic sensibility? Not chasing photorealism, not recreating hardware limitations, just building the kind of stylised, simulation-friendly games that era was known for, without the corners they had to cut.

Part of the fun is this opens up space to experiment with where rendering and engine tech *could* have gone if the industry hadn't spent twenty years optimising for photorealism. A lot of modern engines share the same assumptions because they're all solving the same problem. When you take realism off the table, some of those assumptions might change. 
- Material models don't have to be metallic-roughness.
- Lighting doesn't have to conserve energy.
- Atmosphere doesn't have to simulate scattering.
- Post-processing doesn't have to mimic a camera lens, etc.

So, hopefully there are genuine spaces to play in: artist-driven shading responses, mood-based fog systems, exaggerated physics tuned for feel rather than accuracy, etc. It's a chance to ask "what if?" about the tech itself, not just the games.

## Creative Direction

Games built with Wayfinder should look like games on purpose not just whatever falls out of a PBR pipeline.

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
