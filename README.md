# Wayfinder

> **This project is a personal experiment** to see how far AI-assisted development can go. It's not a production engine, not a portfolio piece, and not representative of any professional work. It's just a fun sandbox for learning what agents can and can't do.

Wayfinder is a C++23 game engine targeting a "fantasy console" - sixth-gen aesthetics (PS2, Dreamcast era) with modern compute, architecture, and tooling. The premise: what would developers from that era have built if dynamic lighting, runtime simulation, and fast iteration were cheap?

See `docs/project_vision.md` for the full rationale.

## Quick Start

```powershell
cmake --preset dev           # configure (Ninja + Clang)
cmake --build --preset debug # build
bin\Debug\journey.exe        # run the sandbox
```

The `dev` preset enables the sandbox, tools, and tests. See `CMakePresets.json` for other presets.

## Repository Layout

| Path | Description |
|------|-------------|
| `engine/wayfinder/` | Engine library — app, assets, core, ecs, gameplay, maths, physics, platform, plugins, rendering, scene |
| `sandbox/journey/` | Playable sandbox with sample assets and config |
| `tools/waypoint/` | Asset validation CLI (`validate`, `validate-assets`, `roundtrip-save`) |
| `tools/` | Dev scripts — `lint.py`, `tidy.py`, `format-fixup.py`, `ci-local.py`, `gh-issues.py` |
| `apps/` | Future desktop apps (editor, project manager, launcher) — stubs only |
| `tests/` | doctest-based test suites (core, rendering, scene, physics) |
| `docs/` | Project documentation |
| `thirdparty/` | Vendored dependencies via CPM |

## Dependencies

| Library | Role |
|---------|------|
| SDL3 | Windowing, input, GPU rendering (SDL_GPU) |
| flecs | ECS and scene world |
| Jolt Physics | 3D physics |
| spdlog | Logging |
| tomlplusplus | Hand-authored data files |
| nlohmann/json | Generated/interchange data |
| ImGui | Immediate-mode UI |
| GLM | Maths |
| fastgltf | glTF import |
| meshoptimizer | Mesh processing |
| Tracy | Profiling |
| Box2D | 2D physics |
| doctest | Unit testing |

## Docs

| File | What it covers |
|------|----------------|
| `docs/project_vision.md` | Fantasy-console premise and design philosophy |
| `docs/workspace_guide.md` | Repository layout, build targets, workflows |
| `docs/render_features.md` | Creating render features and injecting passes |
| `docs/github_issues.md` | Issue tracking conventions and `gh-issues.py` reference |
