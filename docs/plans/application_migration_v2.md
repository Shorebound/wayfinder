# Application Architecture v2 - Migration Guide

**Status:** Planned
**Parent document:** [application_architecture_v2.md](application_architecture_v2.md)
**Last updated:** 2026-04-02

This document maps existing engine code to v2 concepts. Disposable once migration is complete.

---

## Renames

| Existing | v2 Name | Notes |
|---|---|---|
| `SubsystemCollection<TBase>` | `SubsystemRegistry<TBase>` | Same core design. Add dependency ordering and capability-based activation. |
| `GameSubsystem` | `StateSubsystem` | Any application state can have state-scoped subsystems, not just Game. |
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
| `Game` | Remove `GameplayTagRegistry` ownership (moves to app-scoped). `SubsystemCollection<GameSubsystem>` becomes `SubsystemRegistry<StateSubsystem>`. Remove `GetTagRegistry()` (accessed via `IServiceProvider`). |
| `GameStateMachine` | Becomes `StateSubsystem`. Extends generic `StateMachine<TStateId>` if feasible. |
| `PhysicsSubsystem` | Becomes `StateSubsystem`. Declares `RequiresCapability::Simulation`. |
| `GameplayTagRegistry` | Becomes `AppSubsystem`. Accessible in Game via `IServiceProvider`. |
| `GameContext` | Replaced by `IServiceProvider` concept. See [game_framework.md](game_framework.md). |
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
| `IOverlay` | Lightweight decoration interface (attach/detach/update/render/event). |
| Capability tags | `GameplayTag`-based identifiers for activation control. |
| `ApplicationStateMachine` | Hybrid flat + push/pop stack with transition validation. |
| `OverlayStack` | Ordered stack with persistent/transient overlays and capability filtering. |
| `AppSubsystem` | Base class for application-lifetime subsystems. |
| `EngineContext` (new class) | Central context: subsystem registries, state transitions, overlay operations. |
| `GameplayState` | Wraps `Game` into `IApplicationState` lifecycle. |
| `IServiceProvider` | Concept for type-erased service lookup. Keeps `Game` framework-agnostic. |
| `StateMachine<TStateId>` | Generic template shared by application and game state machines. |
| `AppBuilder` config loader | Cached TOML/JSON loading. `builder.LoadConfig<T>(section)`. |
| `AppDescriptor` | Read-only snapshot from `AppBuilder::Finalise()`. Type-keyed registrar store. |
