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

## GitHub Issues & Project Tracking

All engine work is tracked via GitHub Issues. See `docs/github_issues.md` for labels, milestones, relationships, and the GraphQL API reference.

- **Issue relationships use GitHub's native blocked-by/blocking API**, not comments or tasklist syntax. The `addBlockedBy` GraphQL mutation is the correct way to create these.
- **Use `tools/gh-issues/gh-issues.ps1`** to manage relationships from the terminal. It handles node ID lookups automatically:
  ```powershell
  .\tools\gh-issues\gh-issues.ps1 blocked-by 12 7      # #12 is blocked by #7
  .\tools\gh-issues\gh-issues.ps1 blocking 7 12,15     # #7 is blocking #12 and #15
  .\tools\gh-issues\gh-issues.ps1 sub-issue 10 41,42   # #41, #42 are sub-issues of #10
  .\tools\gh-issues\gh-issues.ps1 show 12              # show all relationships for #12
  .\tools\gh-issues\gh-issues.ps1 remove-blocked-by 12 7  # undo: #12 no longer blocked by #7
  .\tools\gh-issues\gh-issues.ps1 remove-sub-issue 10 41  # undo: #41 no longer sub-issue of #10
  ```
- **When completing a task**, close the issue and check if any issues it was blocking are now unblocked.
- **When creating sub-tasks** for a large issue, create new issues and use `sub-issue` to link them.
- **Repo is `Shorebound/wayfinder`**. The `gh` CLI must be authenticated with repo scope.