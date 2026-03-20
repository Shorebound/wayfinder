# Wayfinder

A C++23 game engine and toolchain targeting a "fantasy console" — sixth-gen aesthetics with modern compute, architecture, and tooling. The goal is a clean, extensible engine that is a joy to work with. See `docs/project_vision.md` for the full rationale and `docs/workspace_guide.md` for repo navigation.

Read `.github/AGENTS.md` for known pitfalls, gotchas, and confusion points that agents might encounter when working in the codebase. If you hit something surprising, alert the developer working with you and add it.

## Design Pillars

- **Dynamic over baked** — prefer runtime computation over offline preprocessing.
- **Data-driven** — if it can be a file on disk, it should be. Hot-reload without code changes.
- **Explicit over implicit** — capabilities are checked, passes are validated, nothing silently dropped.
- **Engine is a library** — the game and editor are consumers. The engine never knows who's calling it.
- **Data flows down, events flow up** — systems read data and produce data. Side effects go through event bus or command queue.
- **Composition over inheritance** — component-based design, flat hierarchies, minimal virtual dispatch.
- **Performance with clarity** — efficient code that stays readable. Optimise measured bottlenecks, not hunches.

This project is greenfield. Breaking changes, rewrites, and architectural pivots are welcome. If something is bad, say so and offer the better path. Don't be afraid to throw away code or whole systems if it leads to a cleaner, more maintainable codebase. The goal is a great engine, not to preserve work that isn't up to standard.

## Engineering Standards

- **Modern C++ idioms and design patterns.** Use current best practices, not legacy habits. This applies to language features, architecture, and API design equally.
- **Readable over clever.** Performance matters, but clarity comes first. Optimise hot paths; keep everything else clean and obvious.
- **Modular and extensible where it counts.** Systems that game or editor code touches must have clear extension points. Internal plumbing can be simple.
- **Production-quality framing from the start.** Initial implementations can be minimal, but the surrounding architecture — interfaces, data flow, error handling — must be sound enough to build on without ripping out later. Get the shape right first.
- **Elegant by default.** Strive for APIs and systems that feel clean and satisfying to use. If two approaches are otherwise equal, pick the one that reads better and composes more naturally.
- **Favour the modern and unconventional when it's better.** Don't default to "the way engines usually do it" if a newer technique or a less obvious design is genuinely superior. Be willing to explore — but justify the departure and make sure it still ships.
- **Professional, not precious.** Code should look like it belongs in a shipping engine. If a proposal, plan, or implementation doesn't meet that bar, rework it before moving on.

## Build

CMake 4.0+, MSVC (Visual Studio 17 2022). Presets are the recommended workflow:

```powershell
cmake --preset dev              # sandbox + tools + tests
cmake --build --preset debug    # Debug build
ctest --preset test             # run all tests
```
Configurations are `Debug`, `Development`, and `Shipping` — not the usual `Debug`/`Release`/`RelWithDebInfo`.
Key targets: `wayfinder` (engine lib), `journey` (sandbox), `waypoint` (asset CLI), `wayfinder_render_tests`, `wayfinder_core_tests`.

Adding new source files requires a CMake reconfigure (`CMAKE_SUPPRESS_REGENERATION` is on). Engine sources use `GLOB_RECURSE`, so a reconfigure picks them up automatically.

## Code Style

Formatting is enforced by `.clang-format` (Allman braces, 4-space indent, no column limit).

### Naming

| Element | Convention | Example |
|---------|-----------|---------|
| Types | PascalCase | `TransformComponent`, `RenderGraph` |
| Components | `…Component` suffix | `RenderableComponent`, `CameraComponent` |
| Systems | `…System` suffix | `RenderSystem`, `PhysicsSystem` |
| Interfaces | `I` prefix | `ILogger`, `IUpdatable` |
| Functions | PascalCase | `Initialise()`, `GetPosition()` |
| Members | `m_` prefix | `m_window`, `m_entries` |
| Constants | SCREAMING_SNAKE_CASE | `MAX_ENTITIES`, `DEFAULT_WIDTH` |
| Aliases | PascalCase | `using MeshId = uint32_t;` |
| Template params | `T` prefix | `template<typename TResource>` |

### Function verbs

`IsX` / `HasX` / `WasX` (bool queries), `GetX` / `SetX` (accessors), `CreateX` / `DestroyX` (lifecycle), `LoadX` / `SaveX` (serialisation), `ValidateX` (validation), `OnX` (event handlers), `SubscribeX` / `UnsubscribeX` (event registration).

### Language

- British/Australian spelling throughout: `Initialise`, `Colour`, `Serialise`, `Behaviour`, etc.
- Prefer full words over abbreviations unless widely unambiguous (`Config`, `Params` are fine).
- Use `auto` / `auto*` / `auto&` when the type is obvious from the RHS; be explicit otherwise.
- Prefer C++23 standard library: `std::span`, `std::optional`, `std::variant`, `std::string_view`, `std::format`, concepts and `requires`, RAII and smart pointers.
- Avoid macros; prefer `constexpr`, `inline`, or templates.

### Namespaces

All engine code lives in `Wayfinder`. Subdirectories under `engine/wayfinder/src/` should use sub-namespaces matching their domain:

- `Wayfinder::Rendering`, `Wayfinder::Audio`, `Wayfinder::Physics`, `Wayfinder::UI`, `Wayfinder::Math3D`, etc.

### Comments

- `/** … */` Javadoc-style (with `@brief`, `@param`, `@return`, `@todo`, etc.) for public API and types.
- `///` for inline implementation notes.

### Data Files

- **TOML** for authored content: configuration, gameplay tags, input mappings, materials, scenes, prefabs.
- **JSON** for interchange formats, generated data, and anything benefiting from schema validation.
- Validate data files at load time with clear error messages for authors.