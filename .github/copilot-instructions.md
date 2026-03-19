# Wayfinder

A modern C++23 game engine and toolchain. An engine for a "fantasy" console. 

## Philosophy

The thought experiment is straight-forward: imagine sixth-gen console development culture with 2026-era compute budgets, architecture knowledge, and tooling convenience. The artistic target still belongs to the sixth console generation, so the production values are not about photorealism. What changes is how much of the world can stay alive, dynamic, and responsive at runtime.

The goal is an engine that is a joy to work with: clean code, clear architecture, and a great developer experience. Focus on avoiding technical debt and hacks, even if it means slower initial progress. Design systems that are flexible, extensible, and can grow with the needs of the game. Future-proofing and forward-thinking design are core pillars. The engine should be extensible and moddable by design, while remaining grounded in the project's artistic vision and technical goals.

The role of `.github/AGENTS.md` is to describe common mistakes and confusion points that agents might encounter as they work in this project. If you ever encounter something in the project that surprises or confuses you, please alert the developer working with you and indicate that this is a potential addition to the AGENTS.md file to help prevent future confusion for others. 

## Pillars

- **Dynamic over baked**
    - Prefer runtime computation over offline preprocessing.
    - Example: real-time lighting and shadows instead of lightmaps, global illumination, and reflection probes. Real-time physics and destruction instead of pre-authored animations. Data-driven behavior instead of hardcoded logic.
- **Data-driven design**
    - If it can be a file on disk, it should be so that it can be authored, iterated on, and hot-reloaded without code changes.
    - Example: gameplay tags, input mappings, material definitions, scene hierarchies, and even some systems can be defined in data rather than code.
- **Explicit over implicit**
    - Capabilities are checked, passes are validated. Nothing is silently dropped or assumed.
- **The engine is a library, not an application**
    - The game (and later, the editor) are consumers of the engine. The engine never knows it's being used by an editor.
- **Data flows down, events flow up**
    - Systems read data and produce data. Side effects are explicit and channeled through an event bus or command queue.
- **Composition over inheritance**: 
    - Prefer component-based design and data-driven behavior over deep class hierarchies and virtual dispatch.
- **Modularity and extensibility**: 
    - Design systems to be self-contained and decoupled, with clear interfaces for extension and replacement.
- **Performance with clarity**: 
    - Write efficient code, but not at the cost of readability and maintainability. Optimize bottlenecks when they are identified, not prematurely.
- **Zero API stability guarantees**: 
    - The codebase is free to evolve and pivot as needed. Breaking changes are expected and accepted. 

## Stability

This project is super greenfield. You have full license to propose breaking changes, rewrites, architectural pivots, dependency swaps, or paradigm shifts. It's okay if you change the architecture, rewrite systems, or throw away code that doesn't fit the vision. 

If something is bad, say so and offer the better path. The only way to find the right design is to try things out and iterate quickly. Don't be afraid to break things, as long as you are improving the overall design and moving towards the vision.

## Architecture

The engine follows an explicit, data-oriented design. See `docs/` for full details:

- `docs/project_vision.md` — why the project exists. 
    - Read this when you want to understand the "why" behind design decisions and tradeoffs.
- `docs/future_features.md` — design sketches for future features and systems
- `docs/workspace_guide.md` — how to navigate the workspace, build, and run things

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
- Interfaces: `I` prefix, prefer adjectives when it makes sense to do so
    - Do: `IRenderDevice`, `IAssetLoader`, etc. for core interfaces.
    - Do: `IUpdatable`, `IDrawable`, etc. for component interfaces.
- Functions: PascalCase (e.g. `Initialize`, `Update`, `Render`).
- Members: `m_` prefix (e.g. `m_window`, `m_capabilities`).
- Constants & Macros: SCREAMING_SNAKE_CASE (e.g. `MAX_ENTITIES`, `DEFAULT_SCREEN_WIDTH`).
- Typedefs/Aliases: PascalCase without `T` prefix (e.g. `using MyVector = std::vector<int>`).
- Templates: `T` prefix for template parameters (e.g. `template<typename TMyType>`).
- Prefer British spelling (e.g. `Initialise`, `Colour`) for public APIs, but American spelling is fine for internal code.
- Prefer full words over abbreviations, unless the abbreviation is widely recognized and unambiguous (e.g. `Config`, `Params`).
- Use `auto` (specifically `auto*` or `auto&`) when the type is obvious from the right-hand side, otherwise be explicit.
- Prefer modern C++23+ features and standard library facilities over custom implementations, unless there is a compelling reason not to.
    - `std::span` for array views, `std::optional` for optional values, `std::variant` for tagged unions, `std::string_view` for string parameters, etc.
    - RAII for resource management, smart pointers for ownership semantics, structured bindings for tuple-like types, etc.
- Use concepts and `requires` for template constraints where it improves readability.
- Avoid macros where possible; prefer `constexpr`, `inline` functions, or templates.

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
- `ValidateX` for validation functions (e.g. `ValidateAssets()`, `ValidateScene()`).