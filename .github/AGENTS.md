# AGENTS.md — Known Pitfalls & Gotchas

This file documents common mistakes, confusion points, and non-obvious behaviour that AI agents (and humans) encounter when working in the Wayfinder codebase. If you hit something surprising, add it here.

---

## Build System

- **Test executables are opt-in.** `WAYFINDER_BUILD_TESTS` defaults to `OFF`. Use the `dev` preset or pass `-DWAYFINDER_BUILD_TESTS=ON` explicitly.
- **MSVC vs Clang differences.** The primary local dev compiler is MSVC; cloud agents use Clang with libc++. Code must compile on both. Watch for MSVC-specific pragmas (guard with `#ifdef WAYFINDER_COMPILER_MSVC`) and C++23 feature availability differences between compilers.
- **Local Clang builds.** Use `cmake --preset dev-clang` + `cmake --build --preset clang-debug` to build with Clang on Windows. This catches Clang-specific issues before CI. The build tree goes into `build/clang/`.
- **`-Wmissing-field-initializers` is suppressed.** Designated initialisers that rely on default member initialisers for remaining fields are idiomatic C++20 — don't pad with `= {}`/`= 0`.

## Logging

- **Engine logging uses `std::format`, not fmt.** spdlog is configured with `SPDLOG_USE_STD_FORMAT=ON`, and the engine's `ILogger::LogFormatted` calls `std::vformat` internally. This means `std::formatter<T>` specialisations are what matter — fmt formatters are irrelevant.
- **Log macros:** `WAYFINDER_INFO(category, ...)`, `WAYFINDER_WARNING(...)`, `WAYFINDER_ERROR(...)`, etc. The first argument is always a log category, not a format string.

## Flecs (ECS)

- **Flecs defers mutations inside system callbacks.** `.set<>()` calls inside a system aren't visible to other systems in the same phase. Use different phases (e.g., `PreUpdate` → `OnUpdate`) for producer/consumer pairs.
- **Systems with empty queries must use `.run()`/`iter` callbacks**, not `.each(flecs::entity ...)`. Flecs asserts because `$this` is unavailable on empty queries.
- **`set<T>()` value copy happens AFTER `OnAdd` observers fire.** When `set<T>()` adds a new component to an entity, the sequence is: add component → fire `OnAdd` → copy provided value over the component slot. If an `OnAdd` observer writes to the triggering component (e.g. assigns a runtime handle), the subsequent value copy will overwrite it with the caller's value. **Use `defer_begin()`/`defer_end()` when setting multiple components that an observer monitors.** Deferred mode batches all adds into a single archetype move — values are copied first, then `OnAdd` fires once with all data in place. This also applies to `Entity::AddComponent<T>()`, which calls `set<T>()` internally.
- **Don't use custom copy constructors to reset runtime handles on ECS components.** Flecs copies components between internal tables during archetype changes (e.g. when a new component is added). A copy constructor that resets a runtime handle (like a physics body ID) will silently invalidate live handles during normal Flecs operation. Guard against double-creation in observers instead (e.g. `if (rb.RuntimeBodyId != INVALID) return;`).

## Rendering

- **SDL_GPU SPIR-V binding convention:** Vertex samplers = set 0, vertex UBOs = set 1, fragment samplers = set 2, fragment UBOs = set 3. Combined image samplers need `[[vk::combinedImageSampler]]` + `[[vk::binding(N, SET)]]` on both `Texture2D` and `SamplerState`.
- **`NullDevice` is real.** It's used for headless testing and tools. All `Create*()` methods return `{}` (invalid handles). Code that runs in tests must handle null-device gracefully.

## Module System

- **`Module::Register()` stores factories, not live registrations.** `ModuleRegistry` collects descriptors that are applied once via `ApplyToWorld(flecs::world&)` at startup.

## gh-issues

- **gh-issues is a compiled C++ tool** (source: `tools/gh-issues/src/Main.cpp`). It is built via CMake when `WAYFINDER_BUILD_TOOLS=ON` and output to `bin/<config>/gh-issues.exe`.
- **`ready`, `status`, and `orphans` take no issue number.** Just run `gh-issues ready`, `gh-issues orphans`, or `gh-issues status --milestone "..."` directly.