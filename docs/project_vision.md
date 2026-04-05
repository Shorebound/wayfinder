# Project Vision

Wayfinder is a fantasy-console engine. Mostly based around a silly question: what if early 2000s developers had modern hardware, architectural knowledge, and tooling, but kept the same artistic sensibility? 

Most modern engines are built around photorealistic rendering. That's a reasonable default. It's what the majority of the industry has been working toward for the last two decades. However, it means the rendering pipeline, material models, lighting systems, and GPU budget allocation in those engines all reflect the same set of assumptions. Even in simpler engines, when you want to make a stylised game, you're typically working against the grain. You're bolting NPR techniques on top.

Wayfinder starts from a different assumption: the target is stylised. Not aggressively so, just that sixth-gen and early-00s-PC style that tried to make do with limited hardware and emerging techniques and technology. 

When you make that decision at the engine level rather than at the content level, it hopefully allows for some change. Part of the fun will be that it opens up space to experiment with where rendering and engine tech *could* have gone instead. I can create speculative history timelines or stories about technology improvements that never occured and see if they actually work out.

So we can ask questions about every part of the pipeline:
- Material models don't have to be about physical surface properties.
- Lighting doesn't have to conserve energy and can be simple or even cheat.
- Atmosphere doesn't have to simulate physically-accurate scattering. It can be simulated to whatever standards I want or even be purely arist-driven.
- Post-processing doesn't have to mimic a camera lens, etc.

Hopefully there are genuine spaces to play in. Whether it's artist-driven or purely procedural/simulated, it's a chance to ask "what if?" and genuinely test it.

More importantly, the GPU and CPU budget shifts. Photorealistic rendering is expensive. When you're not spending the majority of your frame time on complex material evaluation, denoising, or high-density geometry streaming, that budget becomes available for other things.

The freed rendering budget goes into making the world feel more alive. Denser populations of NPCs that actually do things. Richer physics interactions. Larger numbers of entities coexisting in the same space without the frame rate collapsing.

This is a core engine goal, not an aspirational stretch target. The ECS architecture, job system, and GPU-driven submission pipeline exist specifically to support high entity counts and dense simulation workloads. The rendering pipeline is designed to be cheap enough that simulation gets a meaningful share of the frame budget.

## Creative Direction

The engine leans into runtime, dynamic systems: lots of dynamic lighting, global illumination, time-of-day, weather, physics, etc. Things that were too expensive to do properly in the early 2000s but aren't anymore. 

We can explore different types of techniques and algorithms: 
- We can take wholesale or repurpose modern ones
- Devise completely novel techniques that fit our (hopefully) unique architecture
- Rediscover forgotten, underutilised or underdeveloped ones that fell by the wayside for one reason or the other
- Find techniques from other disciplines that might become more practical in our engine.

## What You Can Build With It

Wayfinder is a general-purpose engine with specific strengths. The kinds of projects it's well-suited for, roughly in order of ambition:

**General 2D and 3D games.** Anything a typical engine handles - platformers, adventure games, smaller-scope projects. Nothing special here, it just works.

**Sixth-generation-style action games.** Think character-action games, platformers, or stealth games in the vein of what the PS2, Dreamcast and GameCube era produced, but with modern engine architecture underneath. Better draw distances, more responsive controls, richer environments, no compromises from hardware limitations that defined that era. The kind of game where a developer in 2003 knew exactly what they wanted to make but couldn't because the hardware wasn't there.

**Dense multiplayer worlds.** This is the ambitious end. A persistent online game with dense environments and hundreds to potentially low-to-mid thousands of characters in the same area. If we build it from the base up with this in mind and we don't care about photorealism, maybe we can achieve it because the polycounts will be lower, the rendering will be simpler, etc. Maybe we can beat something existing MMO engines struggle with, because those engines are carrying decades of architectural debt and were not designed from scratch with modern hardware in mind.
