# Wayfinder

A C++23 game engine and toolchain targeting a "fantasy console" — sixth-gen aesthetics with modern compute, architecture, and tooling. 
See `docs/project_vision.md` for the full rationale and `docs/workspace_guide.md` for repo navigation.

Read `.github/AGENTS.md` for known pitfalls agents encounter. If you hit something surprising, flag it and add it there.

This project is greenfield. Breaking changes, rewrites, and architectural pivots are welcome. If something is bad, say so and offer the better path. Don't be afraid to question anything. The goal is a great engine, not to preserve our egos.

## Design & Engineering Principles

- **Data-driven** — if it can be a file on disk, it should be. Hot-reload without code changes.
- **Data-oriented design** — organise code and systems around the data they operate on, not around objects or features.
- **Composition over inheritance** — component-based design, flat hierarchies, minimal virtual dispatch.
- **Modular and extensible where it counts.** Systems that game or editor code touches must have clear extension points. Internal plumbing can be simple.

- **Elegant by default.** Strive for APIs and systems that feel clean and satisfying to use. If two approaches are otherwise equal, pick the one that reads better and composes more naturally.
- **Performance with clarity** — efficient code that stays readable. Optimise measured bottlenecks, not hunches.
- **Production-quality framing from the start.** Implementations can be minimal, but the architecture, interfaces, data flow, and error handling must be sound enough to build on. Code should look like it belongs in a shipping engine. We do not want tutorial/prototype code. If a proposal, plan, or implementation doesn't meet that bar, rework it before moving on.

- **Modern design** — current best practices for language, architecture, and APIs; prefer unconventional approaches when they are better, but justify departures from “how engines usually do it.”

- **Dynamic over baked** — prefer runtime computation over offline preprocessing.
- **Data flows down, events flow up** — systems read data and produce data. Side effects go through event bus or command queue.
- **Explicit over implicit** — capabilities are checked, passes are validated, nothing silently dropped.
- **Engine is a library** — the game and editor are consumers. The engine never knows who's calling it.

## Data Files

- **TOML** for hand-authored content: configuration, input mappings, etc. It's more readable and forgiving for humans, and supports comments.
- **JSON** for interchange formats, generated data, and anything benefiting from schema validation such as assets, scene descriptions, render graph definitions, etc. The strictness and tooling support help catch errors early.
- Validate data files at load time with clear error messages for authors.


## Build

CMake 4.0+, Ninja Multi-Config, Clang. Presets are the workflow.
MSVC Build Tools + Windows SDK must be installed (for headers and libraries) but no special shell is needed — `dev` works from any terminal.

```powershell
cmake --preset dev              # sandbox + tools + tests (Ninja + Clang)
cmake --preset dev-msvc         # MSVC validation (needs Developer Shell)
cmake --build --preset debug    # Debug build
ctest --preset test             # run all tests
```
Configurations are `Debug`, `Development`, and `Shipping`.
Key targets: `wayfinder` (engine lib), `journey` (sandbox), `waypoint` (asset CLI), `wayfinder_render_tests`, `wayfinder_core_tests`.

Source files are listed explicitly in CMakeLists.txt. When adding a new `.cpp` or `.h`, add it to the relevant CMakeLists.txt.

## Testing

When implementing a new system, feature, or non-trivial refactor, include or update the tests that cover its important behaviour.
Use doctest. Follow existing patterns in `tests/`.

**Test:** correctness boundaries, round-trip fidelity, contracts between systems, lifecycle/state transitions.
**Skip:** trivial accessors, third-party internals, speculative tests for code that doesn't exist yet.
**Rules:** 
- headless only (no window, no GPU — use Null backend)
- no filesystem outside `tests/fixtures/`. 
- One file per domain, not per class. 
- Names describe behaviour, not functions.

## Validation
- Code must pass its relevant tests, `tools/lint.py --changed` and `tools/tidy.py --changed` to be valid.
- `//NOLINTBEGIN`/`//NOLINTEND` blocks are fine for tests or if solutions are not obvious or solvable (such as inescapable third-party issues).

## Code Style

Formatting is enforced by `.clang-format`.

### Naming

- Types: PascalCase with suffixes for components/systems, and `I` prefix for interfaces.
- Functions: PascalCase with verb prefixes. `IsX` / `HasX` / `WasX` (bool queries), `GetX` / `SetX`, `CreateX` / `DestroyX`, `LoadX` / `SaveX`, `ValidateX`, `SubscribeX` / `UnsubscribeX`, `OnX`.
- Members: `m_` prefix, camelCase.
- Public members: PascalCase
- Constants: SCREAMING_SNAKE_CASE. 
- Aliases: PascalCase. 
- Template params: `T` prefix with descriptive name.

### Language

- British/Australian spelling: `Initialise`, `Colour`, `Serialise`, `Behaviour`, etc.
- Prefer full words over abbreviations unless widely unambiguous: `Config`, `Params`, etc. Avoid `Cfg`, `Prms`, etc.
- Use `auto` / `auto*` / `auto&` when the type is obvious from the RHS; be explicit otherwise.
- Use `using` aliases for complex types, especially function signatures and template instantiations.
- Use idiomatic C++23: coroutines, `std::generator`, `std::span`, `std::optional`, `std::variant`, `std::string_view`, `std::format`, `std::filesystem`, `std::ranges`, etc.
- Prefer value semantics and RAII over raw pointers and manual resource management. Use smart pointers where ownership is shared or non-trivial.
- Concepts and `requires` clauses for template constraints, not SFINAE or `static_assert` checks inside the body.
- Prefer `constexpr`, `inline`, or templates over macros if they can achieve the same goal with better type safety and scoping.
- Where it makes sense, use structured bindings, designated initializers, and aggregate initialization for clarity and brevity.
- For error handling, prefer `Result<T, E>` as the return type. See `engine\wayfinder\src\core\Result.h`.

### Namespaces

All engine code lives in `Wayfinder`. Subdirectories under `engine/wayfinder/src/` should use sub-namespaces matching their domain:
- `Wayfinder::Rendering`, `Wayfinder::Physics`, `Wayfinder::Maths`, etc.

### Comments

- `/** … */` Javadoc-style (with `@brief`, `@param`, `@return`, `@todo`, etc.) for public API and types.
- `///` for inline implementation notes.
- **Provisional code** — when something is clearly not production-ready (tutorial-style glue, hard-coded registrations, default pipelines wired in one place, lists that will grow without a real extension point), mark it so it is not mistaken for the intended architecture. Put this in Javadoc on the function or type, or in a nearby `///` line:
  - **`@prototype`** — scaffolding to unblock development; the real design should be data-driven, modular, or game-owned. Add one line: what should replace it, or a GitHub issue reference (e.g. `#156`).
  - **`@todo`** — planned follow-up; link `#nnn` when an issue exists.
  - **`@fixme`** — incorrect or fragile; fix before treating the work as done.

## Commits, Branches & Issues

All work tracked via GitHub Issues on `Shorebound/wayfinder`. See `docs/github_issues.md` for labels, milestones, relationships, GraphQL notes, and full `gh-issues` examples.

Branches: `<scope>/<issue_number>-<short-kebab-slug>`  
PR titles and commits: `<type>(<scope>): <description>`
- Types: `feat`, `fix`, `refactor`, `test`, `chore`, `docs`, `all`
- Scopes: `rendering`, `core`, `scene`, `assets`, `tools`, `build`, `physics`, `audio`, `platform`

Dependencies use **blocked-by/blocking** and **sub-issues**. Manage them with `tools/gh-issues.py` (`blocked-by`, `blocking`, `sub-issue`, `show`, `tree`, `chain`, `ready`, `status`, `orphans`, and remove helpers).

**Workflow:** `show` / `chain` before starting if blockers matter; `ready` or `status --milestone` to pick work; close issues and check what unblocked; split large work with sub-issues; keep issue bodies lean (template in `docs/github_issues.md`).