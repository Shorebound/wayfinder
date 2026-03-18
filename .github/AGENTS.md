# Wayfinder

C++23 game engine and toolchain. Fantasy-console premise: imagine a sixth-gen console development culture with 2026-era compute budgets, architecture knowledge, and tooling convenience. The artistic target still belongs to the sixth console generation. The production values do not suddenly become film realism. What changes is how much of the world can stay alive, dynamic, and responsive at runtime.

This codebase has zero API stability guarantees. You have full license to propose breaking changes, rewrites, architectural pivots, dependency swaps, or paradigm shifts. If something is bad, say so and offer the better path. Nothing is stapled down.

## Build

```powershell
# Configure (sandbox on by default; add tools/tests/editor as needed)
cmake -S . -B build -DWAYFINDER_BUILD_SANDBOX=ON -DWAYFINDER_BUILD_TOOLS=ON -DWAYFINDER_BUILD_TESTS=ON

# Build primary targets
cmake --build build --config Debug --target journey waypoint wayfinder_render_tests

# Run sandbox
build\bin\Debug\journey.exe

# Validate authored assets
build\bin\Debug\waypoint.exe validate-assets sandbox\journey\assets
build\bin\Debug\waypoint.exe validate sandbox\journey\assets\scenes\default_scene.toml

# Run tests
ctest --test-dir build --build-config Debug
```

CMake sets `CMAKE_SUPPRESS_REGENERATION TRUE` — adding new source files requires a manual reconfigure before MSBuild sees them.

## Code Style

Enforced by `.clang-format` at the workspace root. Allman braces, 4-space indent, no tabs, left-aligned pointers, unlimited line length.

- **Namespace**: All engine code lives in `Wayfinder`. Sub-namespaces for domains (e.g. `Wayfinder::Math3D`).
- **Classes**: PascalCase. Components use `FooComponent` suffix.
- **Members**: `m_` prefix (e.g. `m_window`, `m_capabilities`).
- **Includes**: Relative to `engine/wayfinder/src/` (e.g. `#include "core/Application.h"`).
- **Exports**: `WAYFINDER_API` macro on public class declarations.

## Architecture

The engine follows an explicit, data-oriented design. See `docs/` for full details:

- `docs/project_vision.md` — why the project exists
- `docs/runtime_architecture.md` — runtime boundaries and frame flow
- `docs/data_authoring_and_editor.md` — scenes, prefabs, validation, editor direction
- `docs/architecture_debt.md` — ranked cleanup work
- `docs/implementation_plan.md` — forward roadmap and sequencing

**Key boundaries:**

| Layer | Responsibility |
|-------|---------------|
| `Application` | Process, window, services, main loop |
| `Game` | Owns active `Scene`, progression |
| `Scene` / `SceneDocument` | Flecs world, entity creation, TOML load/save (document parses, scene instantiates) |
| `SceneRenderExtractor` | ECS → `RenderFrame` extraction (renderer never touches Flecs) |
| `Renderer` / `RenderPipeline` | Consumes extracted `RenderFrame`, executes passes against backend |
| `IRenderAPI` | Abstract draw surface (raylib backend today) |

**Runtime modules** (world-transform propagation, active-camera extraction) are registered explicitly via `SceneModuleRegistry`, not discovered implicitly.

**Asset identity** uses `Uuid` with `TypedId<Tag>` wrappers. New ID domains = one tag struct + one `using` alias.

## Conventions

- **Dynamic over baked**: prefer runtime computation over offline preprocessing.
- **Explicit over implicit**: capabilities are checked, passes are validated, entities need `RenderableComponent` to render. Nothing is silently dropped or assumed.
- **Headless-first validation**: asset rules are enforced in CLI tools (`waypoint`), not only in the editor.
- **TOML for authored data, JSON for interchange/diagnostics** — never mix the two.
- **Authored references** use `parent_id` / `prefab_id` (legacy `parent` / `prefab` are load-only fallbacks).
- **Scene hierarchy** authority comes from Flecs `ChildOf` only — no duplicate hierarchy metadata.
- **Runtime-only components** (`WorldTransformComponent`, `ActiveCameraStateComponent`) are populated by Flecs systems, not by the renderer or serialization layer.
- **Flecs pitfall**: `OnUpdate` systems with empty queries must use `.run()`/`.iter()` callbacks, not `.each(flecs::entity ...)`.

## Repository Map

| Path | Purpose |
|------|---------|
| `engine/wayfinder/` | Engine library (static by default) |
| `sandbox/journey/` | Primary playable sandbox + sample assets |
| `sandbox/waystone/` | Separate runtime shell (not yet active) |
| `apps/cartographer/` | Editor (future) |
| `apps/compass/` | Project manager (future) |
| `tools/waypoint/` | Asset validation CLI (active) |
| `tools/beacon/`, `expedition/`, `navigator/`, `surveyor/` | Future tools |
| `tests/` | Engine tests |
| `cmake/` | `WayfinderCommon.cmake` (flags/definitions), `WayfinderDependencies.cmake` (FetchContent) |

## Dependencies

All managed via FetchContent: raylib 5.5, flecs 4.1.5, spdlog 1.15.2, tomlplusplus 3.4.0, nlohmann/json 3.12.0, JoltPhysics 5.5.0, Box2D 3.1.0, Tracy 0.11.1, ImGui 1.91.9b.

## Testing

Custom test harness — no external framework. Tests use `Expect(condition, "message")` macros and return `bool`. Mock backends (e.g. `FakeRenderAPI`, null graphics context) exercise behavior without a live drawing surface. Run with `ctest --test-dir build --build-config Debug`.