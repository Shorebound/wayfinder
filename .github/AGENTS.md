# AGENTS.md — Known Pitfalls & Gotchas

This file documents common mistakes, confusion points, and non-obvious behaviour that AI agents (and humans) encounter when working in the Wayfinder codebase. If you hit something surprising, add it here.

---

## Build System

- **Stale build trees after engine source moves.** If Ninja still tries to compile paths under `engine/wayfinder/src/modules/...` after the plugin migration, the build directory predates the current `CMakeLists.txt`. Reconfigure from scratch: `cmake --preset dev` (or wipe `build/<preset>` and configure again).
- **Test executables are opt-in.** `WAYFINDER_BUILD_TESTS` defaults to `OFF`. Use the `dev` preset or pass `-DWAYFINDER_BUILD_TESTS=ON` explicitly.
- **MSVC vs Clang differences.** The primary local dev compiler is MSVC; cloud agents use Clang with libc++. Code must compile on both. Watch for MSVC-specific pragmas (guard with `#ifdef WAYFINDER_COMPILER_MSVC`) and C++23 feature availability differences between compilers.
- **Linux CI must force-select the right Clang via `update-alternatives --set`.** Installing `clang-22` and registering it with `--install` is not enough on GitHub runners — if alternatives is already in manual mode for an older version, higher priority alone won't switch it. The setup action uses `--set` to force selection and a verification step that fails the build if the resolved major version doesn't match. The CMake preset uses generic `clang`/`clang++` and trusts the environment.
- **Local Clang builds.** Use `cmake --preset dev` + `cmake --build --preset debug` to build with Clang on Windows. This catches Clang-specific issues before CI. The primary build tree goes into `build/dev/`.
- **CI problem matchers.** `.github/workflows/ci.yml` registers `.github/matchers/clang.json` (Linux + static analysis) and `msvc.json` (Windows) so compiler diagnostics appear as annotations on PRs instead of only in raw logs.
- **`tools/tidy.py` only analyses `.cpp` files.** Header diagnostics only appear when a checked translation unit includes that header. If you're cleaning a header-only issue, run tidy on one or more consuming `.cpp` files, not just the header path.
- **`tools/tidy.py` ignores third-party and generated diagnostics by primary location.** If clang-tidy walks into `thirdparty/`, `_deps/`, `build/`, or `shadercompiler/`, those findings are filtered out unless the primary diagnostic location is in repo-owned source (`engine/`, `sandbox/`, `tests/`, `tools/`). This also applies to `tools/lint.py --tidy` and the CI static-analysis job because they share the same tidy wrapper.
- **`-Wmissing-field-initializers` is suppressed.** Designated initialisers that rely on default member initialisers for remaining fields are idiomatic C++20 — don't pad with `= {}`/`= 0`.
- **clang-tidy naming style names are literal.** In `readability-identifier-naming`, `CamelCase` produces PascalCase. Use `camelBack` for the repo's `m_memberName` style.

## Core

- **`InternedString` is not a general-purpose string pool.** It keeps every distinct interned value for the process lifetime. Do not intern high-cardinality or per-frame-unique strings (timestamps, GUIDs, formatted numbers) — use `std::string` / `std::string_view` for those. Stable ids (pass names, graph keys, gameplay tags) are the intended use.

## Logging

- **Engine logging uses `std::format`, not fmt.** spdlog is configured with `SPDLOG_USE_STD_FORMAT=ON`, and the engine's `ILogger::LogFormatted` calls `std::vformat` internally. This means `std::formatter<T>` specialisations are what matter — fmt formatters are irrelevant.
- **Log macros:** `WAYFINDER_INFO(category, ...)`, `WAYFINDER_WARN(...)`, `WAYFINDER_ERROR(...)`, etc. The first argument is always a log category, not a format string.

## Flecs (ECS)

- **Pair targets used in queries are delete-locked (Flecs debug).** In debug builds, Flecs records pair **second** entity ids (e.g. `SceneOwnership(sceneTag)`) while those queries exist; `ecs_delete` on that id asserts if still locked. Wayfinder **does not** `ecs_delete` scene tag entities: after clearing scene-owned entities, tags are `ecs_clear`d and **recycled** per `flecs::world` (see `Scene.cpp`). Do **not** call `world.progress()` from scene shutdown to “flush” Flecs — that advances the whole pipeline one frame. Do not add a second long-lived query on the scene tag without extending the same lifecycle rules.
- **Flecs defers mutations inside system callbacks.** `.set<>()` calls inside a system aren't visible to other systems in the same phase. Use different phases (e.g., `PreUpdate` → `OnUpdate`) for producer/consumer pairs.
- **Systems with empty queries must use `.run()`/`iter` callbacks**, not `.each(flecs::entity ...)`. Flecs asserts because `$this` is unavailable on empty queries.
- **Typed `world.each(...)` can trip clang-analyzer false positives.** We hit `clang-analyzer-core.StackAddressEscape` in Flecs' query-builder internals from a normal typed `world.each` call. If tidy reports that in engine code, prefer iterating entities and fetching components explicitly before considering broader suppression.
- **`set<T>()` value copy happens AFTER `OnAdd` observers fire.** When `set<T>()` adds a new component to an entity, the sequence is: add component → fire `OnAdd` → copy provided value over the component slot. If an `OnAdd` observer writes to the triggering component (e.g. assigns a runtime handle), the subsequent value copy will overwrite it with the caller's value. **Use `defer_begin()`/`defer_end()` when setting multiple components that an observer monitors.** Deferred mode batches all adds into a single archetype move — values are copied first, then `OnAdd` fires once with all data in place. This also applies to `Entity::AddComponent<T>()`, which calls `set<T>()` internally.
- **Don't use custom copy constructors to reset runtime handles on ECS components.** Flecs copies components between internal tables during archetype changes (e.g. when a new component is added). A copy constructor that resets a runtime handle (like a physics body ID) will silently invalidate live handles during normal Flecs operation. Guard against double-creation in observers instead (e.g. `if (rb.RuntimeBodyId != INVALID) return;`).
- **clang-analyzer can false-positive on `world.system(...)` builder internals.** We hit `clang-analyzer-core.StackAddressEscape` through Flecs' `query_builder_i::with` path even with a valid system registration. Prefer a local `NOLINTNEXTLINE(clang-analyzer-core.StackAddressEscape)` at the exact call site over suppressing the warning globally.

## Rendering

- **Shader directory vs working directory.** `EngineConfig` uses a relative `shaders.directory` (default `assets/shaders`). `ShaderManager` resolves relative paths with `SDL_GetBasePath()` (directory containing the executable), **not** the process current working directory. Otherwise launching from Visual Studio / Cursor with CWD set to `sandbox/journey` fails to find SPIR-V next to the binary, and fullscreen/composition pipelines never register (`missing program, pipeline, …`).
- **SDL_GPU SPIR-V binding convention:** Vertex samplers = set 0, vertex UBOs = set 1, fragment samplers = set 2, fragment UBOs = set 3. Slang sources use `Sampler2D<T>` with `[[vk::binding(N, 2)]]` for combined image/sampler slots (matches `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`).
- **`NullDevice` is real.** It's used for headless testing and tools. `CreateTexture()` and `CreateSampler()` return distinguishable valid handles (incrementing counter). All other `Create*()` methods return `{}` (invalid handles). Code that runs in tests must handle null-device gracefully.

## Plugin System

- **Shared game plugins must export `WayfinderGetPluginAPIVersion` (included in `WAYFINDER_IMPLEMENT_GAME_PLUGIN`).** The loader compares the returned value to `Wayfinder::Plugins::WAYFINDER_PLUGIN_ABI_VERSION` before calling create/destroy; bump the constant when the plugin ABI changes.
- **`Plugin::Build()` stores factories, not live registrations.** `PluginRegistry` collects descriptors that are applied once via `ApplyToWorld(flecs::world&)` at startup.
- **Runtime-only vs scene JSON components.** Use a single `RegisterComponent(ComponentDescriptor)` entry: set Apply/Serialise/Validate for authoring; for runtime-only Flecs types, only `Key` + `RegisterFn` (e.g. `world.component<T>()`). `RuntimeComponentRegistry::RegisterComponents` applies all `RegisterFn`s; headless paths without a merged registry call `PluginRegistry::ApplyComponentRegisterFns` instead.
- **Scene plugins are opt-in on the game root plugin.** `Application` does not register transform/camera (or any engine scene plugins). The game’s root `Plugin::Build()` adds `TransformPlugin`, `CameraPlugin`, etc., like Bevy’s explicit `add_plugins(DefaultPlugins)` — a `DefaultPlugins` bundle can wrap that later.

## gh-issues

- **gh-issues is a Python script** (`tools/gh-issues.py`). No build step required — just needs Python 3.10+ and `gh` CLI authenticated with repo scope.
- **`ready`, `status`, and `orphans` take no issue number.** Just run `python tools/gh-issues.py ready`, `python tools/gh-issues.py orphans`, or `python tools/gh-issues.py status --milestone "..."` directly.