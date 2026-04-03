# Application Architecture v2 - Migration Guide

**Status:** Planned
**Parent document:** [application_architecture_v2.md](application_architecture_v2.md)
**Last updated:** 2026-04-02

This document maps existing engine code to v2 concepts. Disposable once migration is complete.

---

## Renames

| Existing | v2 Name | Notes |
|---|---|---|
| `SubsystemCollection<TBase>` | `SubsystemRegistry<TBase>` | Same core design. Add dependency ordering (`DependsOn`), topological sort, and capability-based activation. |
| `Game` | `Simulation` | Stripped to flecs world + scene management. No state machine, no subsystem ownership. Services via `ServiceProvider` concept. |
| `GameSubsystem` | `StateSubsystem` | Any application state can have state-scoped subsystems, not just Simulation. Owned by `EngineContext`. |
| `GameSubsystems` (static accessor) | `StateSubsystems` | Matches scope rename. Same Bind/Unbind pattern. |
| `PluginRegistry` | `AppBuilder` | Builder/wiring API, not a passive registry. Type-keyed registrar store. |
| `Plugin` | `IPlugin` | Interface naming convention (`I` prefix). |

---

## Kept With Modifications

| Existing | Changes |
|---|---|
| `SubsystemCollection<TBase>` | Add `Initialise(SubsystemRegistry&) -> Result<void>` for dependency resolution and error propagation. Add topological sort for init/shutdown (reuse `SystemRegistrar`'s Kahn's algorithm). Add capability-based activation for state-scoped subsystems. Retain predicate filtering and `ShouldCreate()`. |
| `SystemRegistrar` | Stays as a registrar type in `AppBuilder`'s typed store. Topological sort pattern reused for subsystem ordering. |
| `StateRegistrar` | Stays as a registrar type in `AppBuilder`'s typed store. Duplicate detection retained. |
| `TagRegistrar` | Stays as a registrar type in `AppBuilder`'s typed store. Feeds into `GameplayTagRegistry` at init. |
| `EventQueue` | No changes to the queue itself. Dispatch order changes: overlays first (top-down), then active state. |
| `Game` | Renamed to `Simulation`. Remove `GameplayTagRegistry` ownership (moves to app-scoped). Remove `SubsystemCollection<GameSubsystem>` (state subsystems owned by `EngineContext`). Remove `GameStateMachine` ownership (becomes member variable of `IApplicationState`). Remove `TransitionTo()` and `GetCurrentState()`. Keeps flecs world + scene management + `ServiceProvider` access. |
| `GameStateMachine` | No longer a subsystem. Becomes a `StateMachine<InternedString>` member variable in whatever `IApplicationState` needs it (`GameplayState`, `EditorState`, `MainMenuState`). Sub-states registered via `builder.ForState<T>()`. |
| `PhysicsSubsystem` | Becomes `StateSubsystem`. Declares `RequiresCapability::Simulation` and `DependsOn = { typeid(AssetService) }`. |
| `GameplayTagRegistry` | Becomes `AppSubsystem`. Accessible in Simulation via `ServiceProvider`. |
| `GameContext` | Replaced by `ServiceProvider` concept. See [game_framework.md](game_framework.md). |
| `GameState.h` / `RunCondition` helpers | Unchanged. Works within ECS world. |
| `BlendableEffectRegistry` | Becomes a separate `AppSubsystem`. Game volumes push effects; renderer reads them. |
| `ProjectDescriptor` | Stays as stored data in EngineContext. Split into sub-structs if it grows. |

---

## Removals

| Existing | Reason |
|---|---|
| `Layer` / `LayerStack` | Replaced by `IApplicationState` + `IOverlay`. |
| `FpsOverlayLayer` | Rewritten as `FpsOverlay : IOverlay` using debug canvas API. |
| `PluginExport.h` / `PluginLoader.h` / `CreateGamePlugin()` | DLL plugin system for "engine loads game DLL" model. v2 has game owning `main()`. @todo: redesign for hot-reload/mod support. |
| `Plugin::OnStartup()` / `Plugin::OnShutdown()` | Replaced by callback registration during `Build()`. |
| `EngineRuntime` | Decomposed into individual `AppSubsystem` instances. |
| `EngineConfig` (monolithic struct) | Decomposed into per-plugin config. |
| `EngineContext` (existing struct) | Replaced by new `EngineContext` class. The current 27-line non-owning struct is a fundamentally different concept. |
| `BackendConfig` / `PlatformBackend` / `RenderBackend` | Backend selection is plugin composition, not enum dispatch. |
| `Window::Create()` / `Input::Create()` / `Time::Create()` | Concrete types constructed directly by their backend plugin. |

---

## Additions

| New | Purpose |
|---|---|
| `IApplicationState` | State interface with full lifecycle (enter/exit/suspend/resume/update/render/event). |
| `IOverlay` | Lightweight persistent decoration interface (attach/detach/update/render/event). Activated/deactivated at runtime. |
| Capability tags | `GameplayTag`-based identifiers for activation control. |
| `ApplicationStateMachine` | Hybrid flat + push/pop stack with transition validation. |
| `OverlayStack` | Ordered stack with persistent overlays and capability filtering. Registration order = render/input priority. |
| `AppSubsystem` | Base class for application-lifetime subsystems. |
| `EngineContext` (new class) | Central context: subsystem registries, state transitions, overlay operations. Direct reads, deferred writes. |
| `GameplayState` | Wraps `Simulation` into `IApplicationState` lifecycle. Owns sub-state machine. Game UI injected via `IStateUI`. |
| `ServiceProvider` | Concept for type-erased service lookup. Keeps `Simulation` framework-agnostic. |
| `StandaloneServiceProvider` | Registry-based type-erased service container for headless tests/tools. |
| `StateMachine<TStateId>` | Generic flat state machine with transition callbacks. Owned by `IApplicationState` as member variable. |
| `IStateUI` | Per-state, plugin-injected UI provider interface. Replaces concrete `SimulationUI`. Opt-in, registered via `ForState<T>().RegisterStateUI<U>()`. |
| `AppBuilder` config loader | Cached TOML/JSON loading. `builder.LoadConfig<T>(section)`. |
| `AppDescriptor` | Read-only snapshot from `AppBuilder::Finalise()`. Type-keyed registrar store + per-state sub-state descriptors. |
