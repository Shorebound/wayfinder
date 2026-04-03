# Concerns

**Last mapped:** 2026-04-03
**Focus areas:** core, app, plugins, gameplay/simulation

---

## v1-to-v2 Architecture Gap

The largest concern is the distance between the current codebase (v1) and the planned architecture (v2). The v2 plans in `docs/plans/application_architecture_v2.md`, `application_migration_v2.md`, and `game_framework.md` are comprehensive and well-designed, but almost nothing from them is implemented yet.

### What Exists (v1)

- `Application` with `LayerStack` for frame lifecycle
- `EngineRuntime` monolith owning all platform/rendering services
- `Plugin.Build(PluginRegistry&)` with registrar pattern
- `Game` class owning flecs world, subsystems, scene
- `GameStateMachine` as `GameSubsystem` managing `ActiveGameState`
- `GameSubsystem` base class with `SubsystemCollection`
- Minimal `EngineContext` (non-owning struct with 5 fields)
- DLL plugin loading (`PluginLoader`, `PluginExport.h`)

### What v2 Requires (not yet started)

| Concept | Complexity | Dependencies |
|---------|-----------|--------------|
| `IApplicationState` interface | Medium | None |
| `ApplicationStateMachine` (flat + push/pop) | High | `IApplicationState` |
| `IOverlay` / `OverlayStack` | Medium | None |
| Capability system (`GameplayTag`-based) | Medium | `GameplayTag` (exists) |
| `AppBuilder` replacing `PluginRegistry` | High | `IPlugin`, registrar refactor |
| `AppSubsystem` / `StateSubsystem` scopes | Medium | `SubsystemCollection` refactor |
| `EngineContext` v2 (central service access) | High | Subsystem scopes |
| `Simulation` (renamed `Game`) | Medium | `ServiceProvider` concept |
| `ServiceProvider` concept | Low | None |
| `IStateUI` | Low | `IApplicationState` |
| Sub-state machines | Medium | `StateMachine<T>` template |
| `LayerStack` removal | Medium | `ApplicationStateMachine` must exist first |
| `EngineRuntime` decomposition | High | `AppSubsystem` scope must exist |

---

## Coupling Issues

### Scene -> Gameplay/Rendering Coupling

`engine/wayfinder/src/scene/ComponentRegistry.cpp` includes:
- `app/Subsystem.h`
- `gameplay/GameplayTag.h`
- `gameplay/GameplayTagRegistry.h`

Scene (storage tier) should not know about Game (runtime tier). This is a layering violation.

`engine/wayfinder/src/scene/Components.h` includes:
- `rendering/graph/RenderIntent.h`

Scene data model is coupled to render pass routing. Should be decoupled via a separate mapping.

`engine/wayfinder/src/scene/SceneDocument.cpp` includes:
- `rendering/materials/Material.h`

Load-time validation couples authoring to rendering.

### Static Global Accessors

| Accessor | File | Risk |
|----------|------|------|
| `GameSubsystems::Get<T>()` | `app/Subsystem.h` | Assert-only; hidden dependency; no thread safety |
| `BlendableEffectRegistry::GetActiveInstance()` | `volumes/BlendableEffectRegistry.h` | No mutex; init-order fragile |
| `SceneComponentRegistry::Get()` | `scene/ComponentRegistry.h` | No mutex; static init fragile |

These static accessors exist for legitimate reasons (flecs callback signature constraints) but create hidden dependencies and init-order sensitivity.

---

## TODOs and Provisional Code

### @todo Items

| File | Description |
|------|-------------|
| `core/Log.cpp:16` | Static loggers not thread-synchronised; need shared_mutex if multi-threaded logging added |
| `rendering/passes/SceneOpaquePass.cpp:165` | Verify downstream readers handle RGBA16_FLOAT correctly |
| `rendering/materials/Material.h:62` | Refactor AssetLoader/AssetCache to return Result<T> across full pipeline |
| `rendering/materials/Material.cpp:35` | Move colour parsing to Result<LinearColour> |
| `rendering/backend/sdl_gpu/SDLGPUDevice.cpp:858` | Format-aware alignment for texture transfers |
| `rendering/backend/sdl_gpu/SDLGPUDevice.cpp:1356` | Confirm SDL_GPU cycle=true on all backends |
| `volumes/BlendableEffectRegistry.h:88` | Remove static instance once ComponentRegistry supports context parameter |
| `scene/Components.h:137` | Add CullMode, blend overrides, rasteriser state fields |
| `rendering/graph/RenderFrame.h:102` | Future: blend mode, double-sided, stencil ref |

### @prototype Items

| File | Description |
|------|-------------|
| `rendering/passes/SceneOpaquePass.cpp:28` | Hardcoded fallback light; replace with data-driven scene defaults |
| `rendering/pipeline/DefaultFeatures.h:18` | Hardcoded feature registration; replace with data-driven pipeline definition |

---

## Error Handling Inconsistency

**Well-implemented** (Result<T> pattern):
- `PluginLoader::Load()` -> `Result<LoadedPlugin>`
- `Application::Initialise()` -> `Result<void>`
- `EngineRuntime::Initialise()` -> `Result<void>`
- Scene load/save functions

**Legacy patterns** (need migration):
- `AssetCache<T>::Get()` returns `T*` (nullptr on failure, no error details)
- `Mesh::Create()` returns `bool` (no error context)
- `RenderDevice` methods mostly return `bool`
- Asset errors lost after cache lookups

The asset pipeline is the largest gap - `Material.h` has an explicit @todo for this.

---

## Safety Concerns

### Non-owning Raw Pointers

Several subsystems cache non-owning raw pointers that could dangle if the owner is destroyed first:

- `FpsOverlayLayer::m_window = nullptr` (pointer to Window)
- `GameStateMachine::m_world = nullptr` (pointer to flecs world)
- `Game::m_assetService` (non-owning pointer)

In current architecture these lifetimes are well-managed (owner outlives consumer), but the v2 migration with different subsystem scopes will need to verify these relationships hold.

### Type-Erased Payloads

`BlendableEffect.h` uses `std::launder(reinterpret_cast<const T*>(e->Payload))` for type-erased effect data. Type safety depends entirely on caller discipline - the registry registration must match usage. No runtime type checking.

### Jolt Physics Allocation

Jolt-allocated shapes/bodies use Jolt's own allocator. If plugin isolation changes (e.g. DLL boundaries), cross-allocator deallocation would cause UB. Currently safe because everything is in-process, but worth noting for any future modularisation.

---

## Performance Notes

### Known Hotspots

| Area | Issue | Impact |
|------|-------|--------|
| `RenderOrchestrator` feature search | O(n*m) per frame across features | ~120 comparisons/frame at 20 features |
| `SceneRenderExtractor::Extract()` | Separate ECS query loop per component type (4 loops) | Could batch into single multi-component query |
| `TransientResourcePool` | Linear search by width/height/format | Slower as pool grows |
| `FrameAllocator` | Page allocation on demand | First-frame spike with many entities |
| `ComponentRegistry` deserialisation | Intermediate `nlohmann::json` per component | Extra allocation during scene load |

### Allocation Patterns

- `SubsystemCollection::Initialise()` creates `unique_ptr<TBase>` per subsystem (no bulk allocator)
- `RenderOrchestrator::BuildGraph()` iterates `vector<unique_ptr<RenderFeature>>` (pointer indirection per feature)
- Feature search is unordered within the vector

None of these are critical blockers at current scale, but they're worth profiling if entity/feature counts grow significantly.

---

## Thread Safety

**Current model:** Single-threaded. All engine systems assume main-thread execution.

**Explicitly not thread-safe:**
- `Log.cpp` static state (loggers, verbosity, config, generation counter) - explicit @todo
- `BlendableEffectRegistry` get/set instance
- `SceneComponentRegistry::Get()` static
- `GameSubsystems::Get<T>()` static

**v2 design accounts for this:** EngineContext reads are const (safe from any thread), writes are deferred (processed on main thread), frame data is isolated. But the current statics will need attention before any parallelism work.

---

## DLL Plugin System (Removal Planned)

Files marked for removal in v2:
- `engine/wayfinder/src/plugins/PluginExport.h` - ABI version, C-linkage exports
- `engine/wayfinder/src/plugins/PluginLoader.h/cpp` - Runtime DLL loading

Current `EntryPoint.h` uses `CreateGamePlugin()` extern, which the DLL exports. V2 has the game own `main()` and construct plugins directly. The DLL system should be removed once the v2 app architecture is in place.

**Risk:** If the transition is partial (game owns main but DLL loading code remains), the dead code could confuse future contributors.

---

## Documentation Gaps

Public APIs missing `@brief`/`@param`/`@return` docs:

- `rendering/mesh/Mesh.h` - `Create()` return semantics undocumented
- `rendering/backend/RenderDevice.h` - most pure virtuals lack contract docs
- `platform/Window.h` - `Create()` overload distinction unclear
- `assets/AssetService.h` - cache miss error handling undocumented
- `scene/Scene.h` - entity creation/lifetime rules missing
- `rendering/pipeline/RenderOrchestrator.h` - init ordering requirements unstated
- `physics/PhysicsWorld.h` - thread safety assumptions undocumented

---

## Complexity Hotspots

| File | ~Lines | Concern |
|------|--------|---------|
| `scene/ComponentRegistry.cpp` | ~1250 | Monolithic serialisation/validation for ~20+ component types |
| `rendering/backend/sdl_gpu/SDLGPUDevice.cpp` | ~1400 | Full GPU abstraction in single file |
| `rendering/pipeline/SceneRenderExtractor.cpp` | ~500 | Tight coupling to exact ECS component layout |
| `rendering/pipeline/RenderOrchestrator.h` | ~180 | Dense template methods, O(n^2) feature search |
| `volumes/BlendableEffectRegistry.h` | ~200 | Type-erased function pointers, global state |

`ComponentRegistry.cpp` is the most likely to cause merge conflicts and maintenance burden as new component types are added. It would benefit from a registration-based approach (which the v2 `RuntimeComponentRegistry` partially addresses).

---

## Priority Assessment

### Must Address for v2 Migration

1. Introduce `AppSubsystem` / `StateSubsystem` scoping before decomposing `EngineRuntime`
2. Build `AppBuilder` API before new plugin registrations can use it
3. Implement `IApplicationState` + `ApplicationStateMachine` before removing `LayerStack`
4. Standardise `Result<T>` across asset pipeline before new subsystems depend on it

### Should Address Soon

5. Kill `BlendableEffectRegistry` static instance (pass context through DI)
6. Decouple `scene/ComponentRegistry.cpp` from `gameplay/` includes
7. Add `TryGet<T>()` (optional-returning) to subsystem accessors

### Can Defer

8. Thread-safe logging (only needed when multithreading is introduced)
9. Performance hotspots (only relevant at scale)
10. Documentation gaps (address as APIs stabilise post-v2)
