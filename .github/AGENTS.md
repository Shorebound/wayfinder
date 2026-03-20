# AGENTS.md — Known Pitfalls & Gotchas

This file documents common mistakes, confusion points, and non-obvious behaviour that AI agents (and humans) encounter when working in the Wayfinder codebase. If you hit something surprising, add it here.

---

## Build System

- **Test executables are opt-in.** `WAYFINDER_BUILD_TESTS` defaults to `OFF`. Use the `dev` preset or pass `-DWAYFINDER_BUILD_TESTS=ON` explicitly.

## InternedString

- **TOML++ serialisation needs `.GetString()`.** `toml::table::insert_or_assign` won't accept `InternedString` directly — the implicit `operator const std::string&()` isn't enough for TOML++'s template machinery. Always call `.GetString()` when writing to TOML.

## Logging

- **Engine logging uses `std::format`, not fmt.** spdlog is configured with `SPDLOG_USE_STD_FORMAT=ON`, and the engine's `ILogger::LogFormatted` calls `std::vformat` internally. This means `std::formatter<T>` specialisations are what matter — fmt formatters are irrelevant.
- **Log macros:** `WAYFINDER_INFO(category, ...)`, `WAYFINDER_WARNING(...)`, `WAYFINDER_ERROR(...)`, etc. The first argument is always a log category, not a format string.

## Flecs (ECS)

- **Flecs defers mutations inside system callbacks.** `.set<>()` calls inside a system aren't visible to other systems in the same phase. Use different phases (e.g., `PreUpdate` → `OnUpdate`) for producer/consumer pairs.
- **Systems with empty queries must use `.run()`/`iter` callbacks**, not `.each(flecs::entity ...)`. Flecs asserts because `$this` is unavailable on empty queries.
- **One-world architecture.** `Game` owns a single persistent `flecs::world`. Scenes are entity groups within it, not separate worlds. Scene transitions clear entities via `SceneOwnership` relationships, not world destruction.

## Rendering

- **SDL_GPU SPIR-V binding convention:** Vertex samplers = set 0, vertex UBOs = set 1, fragment samplers = set 2, fragment UBOs = set 3. Combined image samplers need `[[vk::combinedImageSampler]]` + `[[vk::binding(N, SET)]]` on both `Texture2D` and `SamplerState`.
- **`NullDevice` is real.** It's used for headless testing and tools. All `Create*()` methods return `{}` (invalid handles). Code that runs in tests must handle null-device gracefully.

## Module System

- **`Module::Register()` stores factories, not live registrations.** `ModuleRegistry` collects descriptors that are applied once via `ApplyToWorld(flecs::world&)` at startup.

## gh-issues

- **gh-issues is a compiled C++ tool** (source: `tools/gh-issues/src/Main.cpp`). It is built via CMake when `WAYFINDER_BUILD_TOOLS=ON` and output to `build/bin/<config>/gh-issues.exe`.
- **`ready`, `status`, and `orphans` take no issue number.** Just run `gh-issues ready`, `gh-issues orphans`, or `gh-issues status --milestone "..."` directly.
- If you encounter surprising behaviour with the tool, note it here.