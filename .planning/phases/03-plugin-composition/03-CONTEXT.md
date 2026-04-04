# Phase 3: Plugin Composition - Context

**Gathered:** 2026-04-05
**Status:** Ready for planning

<domain>
## Phase Boundary

Plugins compose the application through AppBuilder with typed registrar store, dependency ordering, and per-plugin configuration. This phase delivers: AppBuilder with typed registrar store and convenience wrappers, plugin dependency declaration via `PluginDescriptor` (virtual `Describe()` on IPlugin), concept-based plugin groups, lifecycle hook registration (builder lambdas), `AppBuilder::Finalise()` producing immutable AppDescriptor with processed registrar outputs, smart-accumulation validation, full ConfigService (AppSubsystem) with 3-tier config layering and TOML loading, SubsystemRegistry retrofit from frozen-registrar to processed-output pattern, and `Application::AddPlugin<T>()` as the sole public API.

</domain>

<decisions>
## Implementation Decisions

### Plugin Dependencies
- **D-01:** Plugin dependencies declared via virtual `Describe()` on IPlugin returning `PluginDescriptor{.DependsOn}`. Plugin owns its dependencies internally -- no external modification mechanism. `Describe()` has a default implementation returning empty descriptor (most plugins have no deps). Aligns with SubsystemDescriptor's `.DependsOn` pattern for codebase cohesion.
- **D-02:** Plugin deps are internalised. No realistic case for external modification of plugin dependency declarations.

### Plugin Groups
- **D-03:** Concept-based plugin groups. A plugin group is a struct with `Build(AppBuilder&)` that is NOT an IPlugin. Groups are transparent composition -- they don't appear in AppDescriptor as plugins. Room to grow into richer group features (conditional inclusion, platform groups) without polluting the plugin interface. `AppBuilder::AddPlugin` accepts both IPlugin-derived types and group concepts via overload/concept dispatch.

### Lifecycle Hooks
- **D-04:** Lifecycle hooks via builder lambdas: `builder.OnAppReady(lambda)`, `builder.OnStateEnter<T>(lambda)`, `builder.OnStateExit<T>(lambda)`, `builder.OnShutdown(lambda)`. IPlugin interface stays clean (Build-only + Describe). Hooks stored in registrar output, fired by Application at the correct lifecycle points.
- **D-05:** Hook points are app-level + typed state: OnAppReady, OnStateEnter<T>, OnStateExit<T>, OnShutdown. No frame-level hooks -- subsystems and render features cover per-frame work. This keeps hooks for lifecycle boundaries only.

### Per-Plugin Configuration
- **D-06:** 3-tier config layering:
  - Layer 1: Struct defaults (compiled in, always valid baseline)
  - Layer 2: Project config files (`config/<key>.toml`) -- designer/developer authored, committed
  - Layer 3: User overrides (`saved/config/<key>.toml`) -- runtime persistence, gitignored
  Resolution: layer 3 overrides 2, which overrides 1. Each layer is optional.
- **D-07:** One file per config key. `builder.LoadConfig<T>("physics")` maps to `config/physics.toml`. Only loaded if the plugin is present. Cached -- multiple plugins reading the same file parse once.
- **D-08:** Per-plugin override files in `saved/config/`. Mirrors `config/` structure. `saved/config/rendering.toml` contains only overridden keys. Natural "reset one plugin" (delete the file), scoped console writes, clean for moddability.
- **D-09:** Config defaults with logging on load. `LoadConfig<T>("key")` returns `T{}` (struct defaults) if `config/key.toml` doesn't exist. Logs at Info level: "Loaded config/physics.toml" or "No config/physics.toml found, using defaults". Missing files are normal (headless tests, first run).
- **D-10:** Full ConfigService in Phase 3 scope. ConfigRegistrar (build-time collection), ConfigService (AppSubsystem, address-stable storage, TOML loading), `OnConfigReloaded()` virtual stub on subsystem base classes (wired to file-watcher later). User overrides persistence is console system scope (later phase), but the `saved/config/` directory structure is established now.
- **D-11:** Platform-conditional configuration is a future extension point. Not Phase 3 scope. Options noted: platform sections within TOML files, or platform subdirectory (`config/mobile/rendering.toml`). Resolvable at load time in `ConfigService::Load()` without adding a layer to the model.

### AppDescriptor & Validation
- **D-12:** Processed outputs, not frozen registrars. Each registrar has `Finalise() -> Result<OutputType>`. AppDescriptor holds the outputs (e.g., StateGraph, SystemManifest, SubsystemManifest, ConfigManifest), queried by output type: `descriptor.Get<StateGraph>()`. Registrar objects are destroyed after Finalise. Clean compile-time immutability, no leaky builder interfaces, thread-safe by construction.
- **D-13:** Smart-accumulation validation (compiler-style). Within a registrar: accumulate independent errors, skip dependent checks if prerequisites fail. Across registrars: always run all registrars. Cross-registrar validation only runs if all individual registrars passed. Developer sees all independent errors in one pass.
- **D-14:** Application owns AppDescriptor as a value member. EngineContext holds a non-owning reference. Consistent with Phase 2 ownership model (EngineContext is a facade).

### SubsystemRegistry Retrofit
- **D-15:** Retrofit SubsystemRegistry from frozen-registrar to processed-output pattern in Phase 3. `SubsystemRegistry::Finalise()` returns `Result<SubsystemManifest<TBase>>` instead of `Result<void>`. Read-only accessors (`GetSorted()`, `Get<T>()`, `TryGet<T>()`) move to `SubsystemManifest`. Consumers hold manifest, not registry. Mechanical change (~100 LOC), existing tests provide safety net, avoids two patterns at the AppBuilder integration point.

### Future DX Notes (not Phase 3)
- **D-16 (noted):** Future DX cohesion pass could move SubsystemDescriptor.DependsOn to a virtual `Describe()` on subsystem base classes, mirroring the plugin pattern. Not Phase 3 scope -- SubsystemDescriptor designated initialisers work fine.
- **D-17 (noted):** `config_service.md` document was never created (referenced in `console.md` but missing). The design intent is captured in this CONTEXT and in `console.md`'s ConfigService references. Writing a standalone `config_service.md` is a future docs task.

### Agent's Discretion
- Internal storage for AppBuilder's type-keyed registrar container (std::unordered_map<type_index, unique_ptr<IRegistrar>> or similar)
- Exact error message formatting for validation failures (cycle paths, missing deps, etc.)
- Whether convenience methods on AppBuilder (AddState, RegisterSubsystem) are thin inline wrappers or dispatch through a helper
- PluginDescriptor struct fields beyond DependsOn (Name can come from existing GetName() or be on descriptor -- agent chooses)
- ConfigRegistrar internal design (how it tracks type declarations and file keys)
- SubsystemManifest<TBase> internal storage details (vector vs flat_map, etc.)
- How concept dispatch for plugin groups vs IPlugin types is implemented (overloads, if constexpr, requires clause)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Architecture Specifications
- `docs/plans/application_architecture_v2.md` -- AppBuilder design (typed registrar store, convenience wrappers, Finalise -> AppDescriptor), plugin-driven config section, backend selection via plugin composition, startup lifecycle sequence
- `docs/plans/application_migration_v2.md` -- PluginRegistry -> AppBuilder transition, EngineConfig decomposition into per-plugin config
- `docs/plans/console.md` -- ConfigService as AppSubsystem (stable storage, cvar backing, change notification), ConfigRegistrar (build-time config type collection, validation), ConsoleRegistrar pattern (typed registrar example), `saved/` directory structure, `OnConfigReloaded()` flow
- `docs/plans/game_framework.md` -- Simulation, ServiceProvider, state interaction with plugins

### Existing Types (to build on or evolve)
- `engine/wayfinder/src/plugins/IPlugin.h` -- V2 plugin interface (Build(AppBuilder&) only). Phase 3 adds virtual Describe() returning PluginDescriptor.
- `engine/wayfinder/src/plugins/PluginRegistry.h` + `.cpp` -- Current v1.5 plugin registration. TO BE REPLACED by AppBuilder. Study for API expectations but don't preserve the design.
- `engine/wayfinder/src/plugins/registrars/StateRegistrar.h` -- Existing typed registrar. Descriptor{Name, OnEnter, OnExit}, tracks initial state. Migrates into AppBuilder's registrar store.
- `engine/wayfinder/src/plugins/registrars/SystemRegistrar.h` -- Existing typed registrar. Descriptor{Name, Factory, Condition, After[], Before[]}, Kahn's topological sort. Migrates into AppBuilder's registrar store.
- `engine/wayfinder/src/plugins/registrars/TagRegistrar.h` -- Existing typed registrar. Descriptor{Name, Comment}, file paths. Migrates into AppBuilder's registrar store.
- `engine/wayfinder/src/app/SubsystemRegistry.h` -- Phase 2 output. Template SubsystemRegistry<TBase> with SubsystemDescriptor, Finalise(), Kahn's sort. RETROFIT to processed-output pattern (SubsystemManifest).
- `engine/wayfinder/src/app/EngineContext.h` -- Phase 2 output. Non-owning facade. GetAppDescriptor() stub needs real implementation. Holds SubsystemManifest references post-retrofit.
- `engine/wayfinder/src/app/EngineConfig.h` -- Monolithic config (WindowConfig, BackendConfig, ShaderConfig, PhysicsConfig). TO BE REPLACED by per-plugin config via ConfigService.
- `engine/wayfinder/src/app/Application.h` -- Current v1. Takes unique_ptr<Plugin>. Phase 3 target: AddPlugin<T>() as sole public API.
- `engine/wayfinder/src/plugins/ServiceProvider.h` -- C++20 concept + StandaloneServiceProvider. Foundation for Phase 4.

### Supporting Types
- `engine/wayfinder/src/core/Result.h` -- Result<T, TError> alias for std::expected. Used for Finalise() return types.
- `engine/wayfinder/src/core/InternedString.h` -- O(1) equality, std::hash. Used for plugin names, config keys.
- `engine/wayfinder/src/gameplay/Capability.h` -- CapabilitySet and well-known tags.
- `engine/wayfinder/src/gameplay/Tag.h` -- Tag + TagContainer. Capability checks via HasAll/HasAny.

### Test Coverage
- `tests/core/HandleTests.cpp`, `tests/core/ResultTests.cpp` -- Core type test patterns
- `tests/app/SubsystemTests.cpp` -- SubsystemRegistry tests. Must be updated for SubsystemManifest retrofit.
- `tests/plugins/PluginRegistryTests.cpp` (if exists) -- Current plugin registration tests. Reference for expected behaviours.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `SubsystemRegistry<TBase>`: Kahn's topological sort implementation is reusable for plugin dependency ordering. Consider extracting to a shared utility if not already.
- `SystemRegistrar`: Already implements topological sort for ECS system ordering. Same algorithm shape as plugin deps.
- Existing registrar pattern (StateRegistrar, SystemRegistrar, TagRegistrar): These become the first occupants of AppBuilder's typed registrar store. Their existing APIs define what plugins expect.
- `tomlplusplus` (thirdparty): TOML parsing library already available as a dependency. Used for config file loading.

### Established Patterns
- Descriptor-based registration with designated initialisers (SubsystemDescriptor, StateDescriptor)
- Kahn's algorithm for topological sort (SubsystemRegistry, SystemRegistrar)
- Result<T> for error propagation from Finalise() and Initialise()
- Non-owning facade pattern (EngineContext)
- Type-erased storage with std::type_index keys (SubsystemRegistry, ServiceProvider)

### Integration Points
- `Application.h`: Major rewrite target. AddPlugin<T>() replaces current constructor-based plugin injection. Owns AppBuilder during startup, then AppDescriptor for lifetime.
- `PluginRegistry.h/.cpp`: Replaced by AppBuilder. All callsites (currently in Application) migrate.
- `EngineConfig.h`: Replaced by ConfigService + per-plugin config types. Current consumers (subsystems reading EngineConfig fields) migrate to receiving their config via ConfigService or build-time LoadConfig.
- `SubsystemRegistry.h`: Retrofit changes Finalise() return type and consumer access patterns. Tests, EngineContext, and EngineRuntime are affected.
- `CMakeLists.txt`: New files (AppBuilder, AppDescriptor, ConfigService, ConfigRegistrar, PluginDescriptor, SubsystemManifest, etc.) must be added to source lists.
- `EngineContext.h`: GetAppDescriptor() stub becomes real. SubsystemManifest replaces SubsystemRegistry pointers.

</code_context>

<specifics>
## Specific Ideas

- Kahn's topological sort appears in both SubsystemRegistry and SystemRegistrar. Consider extracting to a shared utility in `core/` (e.g., `TopologicalSort.h`) to avoid duplication. Agent's discretion.
- `PluginDescriptor` could carry `.Name` as well as `.DependsOn`, allowing `Describe()` to unify the name + deps declaration. Then `GetName()` can be removed from IPlugin if desired, or kept as a convenience wrapper. Agent's discretion.
- Console.md shows `builder.Registrar<ConsoleRegistrar>()` as the pattern for custom registrars. This validates the typed-registrar-store design from the architecture doc.
- `ProjectDescriptor::ResolveSavedDir()` is mentioned in console.md as returning `ProjectRoot / "saved"`. ConfigService should use this for the `saved/config/` directory.
- `OnConfigReloaded()` should be on both AppSubsystem and StateSubsystem base classes since both may have config-dependent behaviour.

</specifics>

<deferred>
## Deferred Ideas

- Platform-conditional config values (D-11): Not Phase 3. ConfigService API should not block future platform override support.
- Subsystem Describe() cohesion pass (D-16): Future DX pass to add virtual Describe() on subsystem base classes mirroring the plugin pattern.
- config_service.md standalone document (D-17): Write as a docs task to complement console.md.
- User overrides persistence: Console system responsibility (writes to saved/config/<key>.toml). ConfigService provides the read path and directory structure now.
- Hot-reload file watcher: Interface stubbed (OnConfigReloaded), implementation deferred. Watcher is a separate subsystem or ConfigService internal.

</deferred>

---

*Phase: 03-plugin-composition*
*Context gathered: 2026-04-05*
