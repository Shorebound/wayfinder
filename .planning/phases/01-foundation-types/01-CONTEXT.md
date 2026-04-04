# Phase 1: Foundation Types - Context

**Gathered:** 2026-04-03
**Status:** Ready for planning

<domain>
## Phase Boundary

All v2 vocabulary types exist and compile; the dead-end DLL plugin system is removed. This phase delivers: v2 interfaces (IApplicationState, IOverlay, IPlugin, IStateUI), base types (AppSubsystem, StateSubsystem), ServiceProvider concept + StandaloneServiceProvider, StateMachine<TStateId> generic template, capability tag constants, and removal of the DLL plugin system (PluginExport.h, PluginLoader.h, EntryPoint.h, CreateGamePlugin).

</domain>

<decisions>
## Implementation Decisions

### Interface Lifecycle Design
- **D-01:** Mixed return types -- `Result<void>` for OnEnter/OnExit (can fail meaningfully), `void` for per-frame methods (OnUpdate, OnRender, OnEvent, OnSuspend, OnResume).
- **D-02:** Pure virtual abstract base classes for all v2 interfaces (IApplicationState, IOverlay, IPlugin, IStateUI). Deducing `this` used inside concrete implementations where useful, not on the interfaces themselves.
- **D-03:** `EngineContext&` parameter on lifecycle methods. States are constructible without engine knowledge -- OnEnter(EngineContext&) provides access when needed. Easy to test with mock/standalone context.
- **D-04:** `EventQueue&` batch processing via `Drain<T>()`. States receive the queue reference and choose which event types to consume. Consistent with existing EventQueue double-buffer design. One virtual call per frame, not per-event.

### ServiceProvider Contract
- **D-05:** Two-tier API. `Get<T>()` asserts and returns `T&` (caller knows the dependency exists; absence is a configuration bug). `TryGet<T>()` returns `T*` (caller handles absence explicitly). Two-tier makes intent clear at every callsite.
- **D-06:** Type-erased `ServiceLocator` wrapper so Simulation stays a concrete class (not a template). The `ServiceProvider` concept constrains what can be passed in; type erasure is the storage mechanism. One virtual call per `Get<T>()` (called during init and occasionally, not per-entity).
- **D-07:** `StandaloneServiceProvider` uses `type_index` map + `void*`. The type_index key guarantees type safety; void* static_cast is correct by construction. No per-service heap allocation. Industry-standard service locator pattern.

### StateMachine Generics
- **D-08:** `std::function` callbacks (OnEnter, OnExit). Lambdas for small games, `std::bind_front` for class-method binding in larger games. Optional convenience overload `AddState(id, stateObject)` that auto-binds methods via requires clause. Transition-frequency path -- std::function overhead is irrelevant.
- **D-09:** `StateMachine<TStateId>` is flat transitions only (replace current state), callback-based, lightweight. `ApplicationStateMachine` is a separate specialised type managing `IApplicationState` objects with lifecycle (enter/exit/suspend/resume), push/pop modal stack, deferred transitions, and capability integration. They are NOT related by inheritance -- different responsibilities.
- **D-10:** Descriptor-based registration with Finalise-then-run validation. States registered upfront with descriptors (id, callbacks, allowed transitions). `Finalise(initialState)` validates the graph (dangling transitions, unreachable states, initial state validity) and freezes the machine. Pre-validated transitions are O(1) at runtime with no per-transition checks. `@todo` comment for future Unlock()/Revalidate() escape hatch for hot-reload/mod support.
- **D-11:** Generic `TStateId` template parameter -- callers choose keying strategy (InternedString for plugin-registered states, enum class for compile-time-known sub-states, etc.).

### File Organisation
- **D-12:** New v2 types placed in-place in existing domain directories (app/, gameplay/, plugins/). No staging directory. The I-prefix naming convention signals v2 vs v1 types.
- **D-13:** Rename Plugin.h to IPlugin.h, update the class. Clean break -- old references fail to compile (intentional for migration tracking).
- **D-14:** Headers with `#pragma once`, module-ready structure. Minimal includes, aggressive forward declarations, one-to-one file-to-future-module mapping. Convert to C++20 modules when tooling matures. Pure C++ types (core/, v2 interfaces) are primary module candidates; anything touching C libraries (SDL3, flecs, Jolt) stays as headers.
- **D-15:** One file per interface/type. IApplicationState.h, IOverlay.h, IStateUI.h, AppSubsystem.h -- each in its own file.

### Capability System
- **D-16:** Capabilities use `GameplayTagContainer` directly via a `using CapabilitySet = GameplayTagContainer` type alias for domain clarity. Add `AddTags(const GameplayTagContainer&)` merge method to GameplayTagContainer for computing the effective capability set (union of app-level + state-level). No separate capability set class -- GameplayTagContainer already provides HasAll(), HasAny(), AddTag(), RemoveTag(), and hierarchical matching.

### Agent's Discretion
- Internal file naming within domain dirs (e.g. whether IApplicationState.h goes in app/ root or an app/states/ subfolder) -- agent decides based on existing structure
- Exact method signatures for IOverlay and IStateUI beyond the lifecycle pattern established by IApplicationState -- agent follows the same conventions
- Whether to add the convenience `AddState(id, stateObject)` overload in Phase 1 or defer it -- agent decides based on implementation complexity

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Architecture Specifications
- `docs/plans/application_architecture_v2.md` -- Defines the app shell: ApplicationStateMachine, EngineContext, subsystem scoping, overlays, plugins, capability system, lifecycle timeline
- `docs/plans/application_migration_v2.md` -- Exact rename/keep/remove/add transition tables for every v1 type
- `docs/plans/game_framework.md` -- Simulation, GameplayState, ServiceProvider concept, sub-state machines, IStateUI

### Existing Types (to build on or evolve)
- `engine/wayfinder/src/gameplay/GameplayTag.h` -- GameplayTag + GameplayTagContainer + ActiveGameplayTags. Capability system builds on this directly.
- `engine/wayfinder/src/core/InternedString.h` -- Backbone of GameplayTag. Used as state identifiers in StateMachine<InternedString>.
- `engine/wayfinder/src/core/Result.h` -- `Result<T, TError>` alias for std::expected. Used for OnEnter/OnExit return types and subsystem Initialise.
- `engine/wayfinder/src/core/events/EventQueue.h` -- EventQueue + TypedEventBuffer. OnEvent lifecycle method takes EventQueue& for batch processing.
- `engine/wayfinder/src/app/Subsystem.h` -- Current Subsystem/GameSubsystem/SubsystemCollection. GameSubsystem becomes StateSubsystem; SubsystemCollection evolves into SubsystemRegistry.
- `engine/wayfinder/src/gameplay/GameStateMachine.h` -- Current InternedString-keyed state machine. Logic extracted into generic StateMachine<TStateId>.
- `engine/wayfinder/src/plugins/Plugin.h` -- Current Plugin base class. Renamed to IPlugin.h with Build-only pattern.

### Types to Remove
- `engine/wayfinder/src/plugins/PluginExport.h` -- DLL export macros (WAYFINDER_IMPLEMENT_GAME_PLUGIN)
- `engine/wayfinder/src/plugins/PluginLoader.h` + `.cpp` -- Runtime DLL loading (LoadLibrary/dlopen)
- `engine/wayfinder/src/app/EntryPoint.h` -- Macro-based entry point calling CreateGamePlugin()

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `GameplayTag` + `GameplayTagContainer`: Capability system uses these directly. HasAll/HasAny for activation checks. AddTags for capability set merging.
- `InternedString`: O(1) equality, std::hash specialisation. Natural key type for StateMachine<InternedString> and plugin/subsystem identifiers.
- `Result<T, TError>`: std::expected alias with Error type and MakeError helper. Used for all failable lifecycle methods.
- `EventQueue` + `TypedEventBuffer`: Double-buffered, per-type-batch dispatch. OnEvent takes EventQueue& reference.
- `SubsystemCollection<TBase>`: Template container with Register<T>/Get<T>/Initialise/Shutdown. Evolves into SubsystemRegistry with dependency ordering and capability activation.
- `GameStateMachine`: InternedString-keyed, callback-based state transitions. Logic basis for generic StateMachine<TStateId>.
- `Handle.h` concept pattern (`OpaqueHandleTag`): Reusable pattern for ServiceProvider concept definition.

### Established Patterns
- Virtual base classes with lifecycle methods (Layer, Plugin, Subsystem) -- v2 interfaces follow the same pattern
- SubsystemCollection Register<T>/Get<T> API -- SubsystemRegistry keeps this ergonomic pattern
- doctest with headless testing (no window, no GPU, Null backend)
- `std::function` callbacks in GameStateMachine for OnEnter/OnExit -- same pattern in StateMachine<TStateId>

### Integration Points
- `EngineContext` struct (27-line, non-owning) will be replaced by a new EngineContext class -- all current consumers need updating
- `Plugin::Build(PluginRegistry&)` becomes `IPlugin::Build(AppBuilder&)` -- PluginRegistry is renamed in Phase 3
- `Game` class owns GameStateMachine, subsystems, and world -- these are separated in the v2 migration
- CMakeLists.txt at `engine/wayfinder/CMakeLists.txt` lists source files explicitly -- every new/renamed/removed file must be updated there

</code_context>

<specifics>
## Specific Ideas

- ServiceLocator type erasure pattern chosen explicitly to keep Simulation as a concrete class, avoiding template viral spread
- The convenience overload `AddState(id, stateObject)` using a requires clause for auto-binding OnEnter/OnExit methods is a nice QoL feature -- implement if straightforward, defer if complex
- ApplicationStateMachine is deliberately NOT derived from StateMachine<TStateId> -- they solve different problems (lightweight flat FSM vs full lifecycle orchestrator with push/pop and capabilities)
- Capability constants use `GameplayTag::FromName()` with `inline const` in a `Capability` namespace, following the pattern from the v2 architecture doc

</specifics>

<deferred>
## Deferred Ideas

None -- discussion stayed within phase scope.

</deferred>

---

*Phase: 01-foundation-types*
*Context gathered: 2026-04-03*
