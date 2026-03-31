# AGENTS.md - Known Pitfalls & Gotchas

This file documents repo-specific traps that are easy to miss even after reading nearby code. General workflow and build guidance belongs in `docs/workspace_guide.md`. API preferences and style guidance belong in `.github/copilot-instructions.md`.

## Logging

- **Engine logging uses `std::format`, not fmt.** spdlog is configured with `SPDLOG_USE_STD_FORMAT=ON`, and the engine's `ILogger::LogFormatted` calls `std::vformat` internally. This means `std::formatter<T>` specialisations are what matter -- fmt formatters are irrelevant.

## Flecs (ECS)

- **Pair targets used in queries are delete-locked (Flecs debug).** In debug builds, Flecs records pair **second** entity ids (e.g. `SceneOwnership(sceneTag)`) while those queries exist; `ecs_delete` on that id asserts if still locked. Wayfinder **does not** `ecs_delete` scene tag entities: after clearing scene-owned entities, tags are `ecs_clear`d and **recycled** per `flecs::world` (see `Scene.cpp`). Do **not** call `world.progress()` from scene shutdown to “flush” Flecs — that advances the whole pipeline one frame. Do not add a second long-lived query on the scene tag without extending the same lifecycle rules.
- **Flecs defers mutations inside system callbacks.** `.set<>()` calls inside a system aren't visible to other systems in the same phase. Use different phases (e.g., `PreUpdate` → `OnUpdate`) for producer/consumer pairs.
- **Systems with empty queries must use `.run()`/`iter` callbacks**, not `.each(flecs::entity ...)`. Flecs asserts because `$this` is unavailable on empty queries.
- **Typed `world.each(...)` can trip clang-analyzer false positives.** We hit `clang-analyzer-core.StackAddressEscape` in Flecs' query-builder internals from a normal typed `world.each` call. If tidy reports that in engine code, prefer iterating entities and fetching components explicitly before considering broader suppression.
- **`set<T>()` value copy happens AFTER `OnAdd` observers fire.** When `set<T>()` adds a new component to an entity, the sequence is: add component → fire `OnAdd` → copy provided value over the component slot. If an `OnAdd` observer writes to the triggering component (e.g. assigns a runtime handle), the subsequent value copy will overwrite it with the caller's value. **Use `defer_begin()`/`defer_end()` when setting multiple components that an observer monitors.** Deferred mode batches all adds into a single archetype move — values are copied first, then `OnAdd` fires once with all data in place. This also applies to `Entity::AddComponent<T>()`, which calls `set<T>()` internally.
- **Don't use custom copy constructors to reset runtime handles on ECS components.** Flecs copies components between internal tables during archetype changes (e.g. when a new component is added). A copy constructor that resets a runtime handle (like a physics body ID) will silently invalidate live handles during normal Flecs operation. Guard against double-creation in observers instead (e.g. `if (rb.RuntimeBodyId != INVALID) return;`).
- **clang-analyzer can false-positive on `world.system(...)` builder internals.** We hit `clang-analyzer-core.StackAddressEscape` through Flecs' `query_builder_i::with` path even with a valid system registration. Prefer a local `NOLINTNEXTLINE(clang-analyzer-core.StackAddressEscape)` at the exact call site over suppressing the warning globally.

## Rendering

- **Shader directory vs working directory.** `EngineConfig` uses a relative `shaders.directory` (default `assets/shaders`). `ShaderManager` resolves relative paths with `SDL_GetBasePath()` (directory containing the executable), **not** the process current working directory. Otherwise, launching from Visual Studio / Cursor with CWD set to `sandbox/journey` fails to find SPIR-V next to the binary, and fullscreen/composition pipelines never register (`missing program, pipeline, …`).
- **SDL_GPU SPIR-V binding convention:** Vertex samplers = set 0, vertex UBOs = set 1, fragment samplers = set 2, fragment UBOs = set 3. Slang sources use `Sampler2D<T>` with `[[vk::binding(N, 2)]]` for combined image/sampler slots (matches `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`).
- **`NullDevice` is real.** It's used for headless testing and tools. `CreateTexture()` and `CreateSampler()` return distinguishable valid handles (incrementing counter). All other `Create*()` methods return `{}` (invalid handles). Code that runs in tests must handle null-device gracefully.

## Plugin System

- **`Plugin::Build()` stores factories, not live registrations.** `PluginRegistry` collects descriptors that are applied once via `ApplyToWorld(flecs::world&)` at startup.
- **Runtime-only vs scene JSON components.** Use a single `RegisterComponent(ComponentDescriptor)` entry: set Apply/Serialise/Validate for authoring; for runtime-only Flecs types, only `Key` + `RegisterFn` (e.g. `world.component<T>()`). `RuntimeComponentRegistry::RegisterComponents` applies all `RegisterFn`s; headless paths without a merged registry call `PluginRegistry::ApplyComponentRegisterFns` instead.
- **Scene plugins are opt-in on the game root plugin.** `Application` does not register transform/camera (or any engine scene plugins). The game’s root `Plugin::Build()` adds `TransformPlugin`, `CameraPlugin`, etc., like Bevy’s explicit `add_plugins(DefaultPlugins)` — a `DefaultPlugins` bundle can wrap that later.