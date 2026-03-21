# Wayfinder

Wayfinder is a C++23 game engine and toolchain experiment built around a simple question: what if a sixth-generation console era engine had modern compute budgets, modern tooling expectations, and none of the pressure to chase photorealism?

The project aims for stylised, simulation-friendly worlds that feel closer to PS2, Dreamcast, and early-2000s console art direction than to physically accurate rendering. The point is not to reproduce historical constraints. The point is to explore what developers from that era might have built if dynamic lighting, runtime simulation, and aggressive iteration were cheap.

## What Wayfinder Is Trying To Prove

- dynamic systems are often more interesting than baked ones
- expressive visuals matter more than realism-first pipelines
- simple runtime rules scale better than fragile content tricks
- a retro visual target can still benefit from modern engine architecture
- tools should reduce friction without dictating the creative direction

That leads to a renderer and runtime that prefer runtime computation over bake-heavy workflows, stylisation over physical correctness, and stable iteration loops over expensive offline setup.

## Current State

Wayfinder is in active development. The parts that matter today are the runtime foundation and the authored data path.

Implemented and usable now:

- `wayfinder` engine library
- `journey` sandbox executable
- Flecs-based scene runtime
- authored TOML scenes and prefabs
- typed prefab and material asset scanning
- scene validation and roundtrip save through `waypoint`
- explicit runtime module registration for scene systems such as world-transform propagation and active camera extraction

Present but not yet full products:

- `cartographer` editor shell
- `compass` project manager
- `waystone` runtime shell
- `surveyor`, `expedition`, `beacon`, and `navigator`

## Quick Start

Configure the project:

```powershell
cmake -S . -B build -DWAYFINDER_BUILD_SANDBOX=ON -DWAYFINDER_BUILD_TOOLS=ON
```

Build the sandbox and the current tool:

```powershell
cmake --build build --config Debug --target journey waypoint
```

Run the sandbox:

```powershell
build\bin\Debug\journey.exe
```

Validate the checked-in sandbox assets:

```powershell
build\bin\Debug\waypoint.exe validate-assets sandbox\journey\assets
build\bin\Debug\waypoint.exe validate sandbox\journey\assets\scenes\default_scene.toml
build\bin\Debug\waypoint.exe roundtrip-save sandbox\journey\assets\scenes\default_scene.toml build\bin\Debug\assets\scenes\default_scene_roundtrip.toml
```

By default, the top-level build enables the sandbox and leaves standalone tools disabled unless `WAYFINDER_BUILD_TOOLS=ON` is supplied.

## Technology Choices

- `SDL3` provides windowing, input, events, and GPU rendering (via SDL_GPU)
- `flecs` is the current ECS and scene world foundation
- `tomlplusplus` backs hand-authored runtime assets
- `nlohmann/json` is reserved for generated, interchange, or diagnostic data
- `spdlog` provides logging
- `JoltPhysics` is the intended near-term 3D physics direction
- `ImGui` provides immediate-mode UI (editor integration ready)
- `Box2D` and `Tracy` are available in the dependency plan but are not yet central to the checked-in workflow
- `doctest` provides unit testing

## Repository Map

- `engine/wayfinder/` contains the engine library
- `sandbox/journey/` contains the current playable runtime sandbox and sample assets
- `sandbox/waystone/` is a separate runtime shell that is not yet an active workflow target
- `apps/` holds future desktop applications such as the editor and project manager
- `tools/` holds standalone command-line and diagnostics tools
- `docs/` holds the project documentation set

## Documentation Map

- `docs/project_vision.md` explains the fantasy-console premise and design philosophy
- `docs/workspace_guide.md` explains the repository layout, build targets, and current workflows
- `docs/runtime_architecture.md` explains the runtime boundaries and current frame flow
- `docs/data_authoring_and_editor.md` explains scenes, prefabs, validation, and editor direction
- `docs/architecture_debt.md` ranks the main cleanup work ahead of editor growth
- `docs/implementation_plan.md` explains the forward roadmap and sequencing

Start with `project_vision.md` if you want to understand why the project exists. Start with `workspace_guide.md` if you want to build it or find your way around the repository.