# Wayfinder

C++23 game engine and toolchain. An engine for a "fantasy" console. The thought experiment is straight-forward: imagine sixth-gen console development culture with 2026-era compute budgets, architecture knowledge, and tooling convenience. The artistic target still belongs to the sixth console generation, so the production values are not about photorealism. What changes is how much of the world can stay alive, dynamic, and responsive at runtime.

This codebase has zero API stability guarantees. You have full license to propose breaking changes, rewrites, architectural pivots, dependency swaps, or paradigm shifts. If something is bad, say so and offer the better path. Nothing is stapled down.

Our goal is to create a modern, modular, data-driven, elegant and well-architected engine that is easily extensible for developers using it in their games. 

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

Enforced by `.clang-format` at the workspace root. Short or empty blocks are one-liners.

### General
- Types: PascalCase (structs, classes, enums, aliases).
- Components: `Component` suffix.
    - Do: `TransformComponent`, `RenderableComponent`, `CameraComponent`.
- Systems: `System` suffix.
    - Do: `RenderSystem`, `PhysicsSystem`, `InputSystem`.
- Interfaces: `I` prefix, prefer adjectives when it makes sense
    - Do: `IRenderDevice`, `IAssetLoader`, etc. for core interfaces.
    - Do: `IUpdatable`, `IDrawable`, etc. for component interfaces.
- Functions: PascalCase (e.g. `Initialize`, `Update`, `Render`).
- Members: `m_` prefix (e.g. `m_window`, `m_capabilities`).
- Constants & Macros: SCREAMING_SNAKE_CASE (e.g. `MAX_ENTITIES`, `DEFAULT_SCREEN_WIDTH`).
- Typedefs/Aliases: PascalCase without `T` prefix (e.g. `using MyVector = std::vector<int>`).
- Templates: `T` prefix for template parameters (e.g. `template<typename TMyType>`).
- Prefer British spelling (e.g. `Initialise`, `Colour`) for public APIs, but American spelling is fine for internal code.
- Use `auto` (specifically `auto*` or `auto&`) when the type is obvious from the right-hand side, otherwise be explicit.

### Namespaces
All engine code lives in `Wayfinder`.
Sub-namespaces for domains (e.g. `Wayfinder::Audio`, `Wayfinder::Physics`, `Wayfinder::UI`)

### Comments
- Doxygen-style comments for public APIs (`/** */` with `@param`, `@return`, etc.).
    - Do:
        ```cpp
        /**
        * @brief Initializes the rendering device with the specified parameters.
        * @param windowHandle The native handle to the application window.
        * @param width The width of the rendering surface.
        * @param height The height of the rendering surface.
        * @return True if initialization succeeded, false otherwise.
        */
        ```
    - Do:
        ```cpp
        /**
        * @struct RenderableComponent
        * @brief A component that marks an entity as renderable and holds its mesh and material references.
        * @todo Add support for multiple meshes/materials per entity in the future.
        */
        ```
- `///` for internal comments and explanations of non-obvious code.

### Functions
- `IsX`, `HasX`, `WasX` for boolean queries (e.g. `IsRunning`, `HasFocus`, `WasPressed`).
- `GetX`, `SetX` for accessors/mutators (e.g. `GetPosition()`, `SetColor()`).
- `SubscribeX`, `UnsubscribeX` for event subscription (e.g. `SubscribeOnKeyPressed()`, `UnsubscribeOnKeyPressed()`).
- `OnX` for event handlers (e.g. `OnKeyPressed()`, `OnCollision()`).
- `CreateX`, `DestroyX` for factory/destruction functions (e.g. `CreateEntity()`, `DestroyEntity()`).
- `LoadX`, `SaveX` for serialization functions (e.g. `LoadScene()`, `SaveScene()`).


## Pillars

- **Dynamic over baked**: prefer runtime computation over offline preprocessing.
- **Explicit over implicit**: capabilities are checked, passes are validated, entities need `RenderableComponent` to render. Nothing is silently dropped or assumed.
- **Headless-first validation**: asset rules are enforced in CLI tools (`waypoint`), not only in the editor.
- **TOML for authored data, JSON for interchange/diagnostics** — never mix the two.
- **Hot-reload everything & data-driven design**: if it can be a file on disk, it should be. Materials, entity archetypes, input mappings, AI behavior trees, dialogue — all data, all loadable, all hot-reloadable.
- **The engine is a library, not an application**: the game (and later, the editor) are consumers of the engine. The engine never knows it's being used by an editor.
- **Data flows down, events flow up**: systems read data and produce data. Side effects are explicit and channeled through an event bus or command queue.
- **Composition over inheritance**: prefer component-based design and data-driven behavior over deep class hierarchies and virtual dispatch.
- **Modularity and extensibility**: design systems to be self-contained and decoupled, with clear interfaces for extension and replacement.
- **Performance with clarity**: write efficient code, but not at the cost of readability and maintainability. Optimize bottlenecks when they are identified, not prematurely.

## Architecture

The engine follows an explicit, data-oriented design. See `docs/` for full details:

- `docs/project_vision.md` — why the project exists
- `docs/runtime_architecture.md` — runtime boundaries and frame flow
- `docs/data_authoring_and_editor.md` — scenes, prefabs, validation, editor direction
- `docs/architecture_debt.md` — ranked cleanup work
- `docs/implementation_plan.md` — forward roadmap and sequencing

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
