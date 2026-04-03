# Project Research Summary

**Project:** Wayfinder v2 Application Architecture Migration
**Domain:** C++23 game engine monolithic-to-modular migration
**Researched:** 2026-04-03
**Confidence:** HIGH

## Executive Summary

This migration restructures ~7500 LOC of Wayfinder's engine application shell from a monolithic design (EngineRuntime + LayerStack + Game) to a plugin-composed architecture (EngineContext + ApplicationStateMachine + OverlayStack + Simulation). The core technical challenge is a *type-erased service composition* problem, and C++23 provides a coherent toolkit for it: concepts for compile-time polymorphism on `ServiceProvider`, `std::expected` (already in use as `Result<T>`) for fallible init chains, `std::flat_set` for cache-friendly capability tag sets, and deducing `this` for CRTP-free mixins. No new external libraries are required -- every pattern builds on the C++23 standard library and existing engine primitives (`GameplayTag`, `InternedString`, `Handle<T>`, `SubsystemCollection`).

The recommended approach is a tiered incremental migration (controlled strangler fig). v2 components are built bottom-up in dependency order alongside v1 code. Each tier is independently testable. v1 code is removed only after v2 replacements are proven. The critical insight from architecture research: the v2 components form a strict dependency DAG with four tiers -- foundation types (zero deps), subsystem infrastructure (spine), orchestration (state machine/overlays/simulation), and integration (concrete states + frame loop switchover). Building out of order creates unnecessary scaffolding or circular dependencies.

The top risks are: (1) state machine re-entrancy during transitions corrupting the state stack, (2) subsystem init-order dependencies breaking when plugins register in different orders, (3) the frame loop switchover being non-atomic and leaving the engine in an incoherent state, and (4) capability set emptiness during state transitions causing visual flicker and unnecessary resource churn. All four are preventable with explicit design decisions documented in the pitfalls research and must be addressed in their respective implementation phases -- not patched after the fact.

## Key Findings

### Recommended Stack

All patterns are pure C++23 -- no new library dependencies. The migration extends existing engine foundations into the application shell.

**Core patterns:**
- **Concept-based `ServiceProvider`**: Compile-time polymorphism for `Simulation` dependency injection. Two concrete providers (EngineContext adapter for live, StandaloneServiceProvider for tests). Static dispatch, no vtable overhead, clear concept-violation diagnostics.
- **`GameplayTag`-based capability system**: `std::flat_set<GameplayTag>` with `std::ranges::all_of` for set containment. Leverages existing `InternedString`-backed tags with O(1) equality. Hierarchical matching via `IsChildOf`.
- **`Result<void>` lifecycle methods**: `IApplicationState::OnEnter()` returns `Result<void>`, enabling graceful transition failure (rollback to previous state). Hot paths (`Update`/`Render`) remain void.
- **Type-indexed registrar container**: `AppBuilder` holds domain-specific registrars keyed by `std::type_index`. Extensible without modifying AppBuilder itself. `if constexpr` + `std::derived_from` for compile-time registration routing.
- **Topological sort for init order**: Reuse Kahn's algorithm already proven in `SystemRegistrar.cpp`. `SubsystemRegistry` declares `DependsOn` explicitly; shutdown is reverse topological order.
- **Generic `StateMachine<TStateId>`**: `std::optional` for deferred transitions, `StateIdentifier` concept constraint, `std::flat_map` for small state counts.

**Toolchain note:** Verify `std::flat_set`/`std::flat_map` and `std::ranges::to<>` availability on the project's Clang/libc++ version before use. Trivial fallbacks exist (`std::vector` + sort).

### Expected Features

**Must have (table stakes):**
- Application state machine with enter/exit lifecycle, deferred transitions, suspend/resume for modal push/pop
- Plugin-as-sole-extension-unit with dependency declaration and build-time ordering
- Scoped subsystem lifetimes (AppSubsystem/StateSubsystem) with dependency-ordered init/shutdown
- Abstract-type resolution (access by interface, not concrete type)
- Overlay system separate from states, with input consumption and runtime toggle
- Headless simulation (no window/GPU) for tests and CLI tools
- Engine-as-library (game owns `main()`)

**Differentiators (competitive advantage):**
- Unified `GameplayTag`-based capability activation gating subsystems, overlays, and render features through one mechanism -- no other engine does this
- Push/pop negotiation protocol (`BackgroundPreferences` + `SuspensionPolicy`) for modal states
- Plugin-injected per-state UI (`IStateUI`) with lifecycle mirroring state lifecycle
- Typed registrar store allowing game code to add custom domain registrars without engine changes
- `ServiceProvider` concept enabling identical simulation code in live, CLI, and test contexts

**Defer (not this milestone):**
- DLL/shared library plugin loading (static linking only)
- Hot-reload of plugins or subsystems
- Full editor implementation (EditorState stub only)
- UI toolkit/widget system (IStateUI interface only)
- Multi-threaded frame loop (design for thread-readiness, implement later)
- Config hot-reload (stub callback interfaces)
- Scripting integration, undo/redo, automatic plugin discovery

### Architecture Approach

The migration uses a tiered incremental strategy with four build tiers and a cleanup tier. v2 types are built alongside v1 with clean separation until the integration phase. The frame loop switchover is atomic -- no gradual migration of the loop is possible.

**Major components and their tiers:**

1. **Tier 0 - Foundation** (zero dependencies): `IApplicationState`, `IOverlay`, `IPlugin`, `IStateUI`, `ServiceProvider` concept, `StandaloneServiceProvider`, `StateMachine<T>`, `AppSubsystem`/`StateSubsystem` base classes, capability tag constants
2. **Tier 1 - Infrastructure**: `SubsystemRegistry<TBase>` (dependency ordering + capability gating), `EngineContext` v2 (owns registries, typed getters), `AppBuilder` (replaces PluginRegistry), `AppDescriptor` (immutable snapshot)
3. **Tier 2 - Orchestration**: `Simulation` (headless-capable, replaces Game), `ApplicationStateMachine` (flat + push/pop, deferred transitions), `OverlayStack` (capability-filtered)
4. **Tier 3 - Concrete + Integration**: `GameplayState`, `EditorState` stub, `FpsOverlay`, EngineRuntime decomposition into AppSubsystems, Application rewrite, Journey update
5. **Tier 4 - Cleanup**: Remove LayerStack, Layer, PluginExport/Loader, old EngineRuntime, old EngineContext, Game class, DLL plugin system

### Critical Pitfalls

1. **State machine re-entrancy** -- `OnEnter`/`OnExit` callbacks requesting another transition during an active transition corrupts the state stack and double-frees subsystems. **Prevent:** Guard flag + deferred-only transitions. Assert on `RequestTransition` during active callback. Test A->B->C chains.

2. **Subsystem init-order without explicit `DependsOn`** -- Registration order works today but silently breaks when plugins register differently. **Prevent:** Implement topological sort in `SubsystemRegistry` *before* decomposing EngineRuntime. Detect cycles in `Finalise()`. Shutdown in reverse topological order.

3. **Frame loop non-atomic switchover** -- The v1 loop touches everything (events, simulation, rendering). Partial dismantling leaves neither loop coherent. **Prevent:** Write entire v2 loop in new method, swap atomically in one commit. Feature flag acceptable during transition.

4. **Empty capability set during state transitions** -- Between `OnExit(A)` and `OnEnter(B)`, state capabilities are briefly empty, causing unnecessary subsystem churn and overlay flicker. **Prevent:** Batch the capability transition -- compute target set before teardown, fire deltas only after both exit and enter complete.

5. **Static accessor migration breaks flecs callbacks** -- `GameSubsystems::Get<T>()` exists because flecs system callbacks can't accept injected context. **Prevent:** Keep the static accessor pattern (renamed to `StateSubsystems`) for the entire milestone. Rename atomically across engine + tests.

## Implications for Roadmap

### Phase 1: Foundation Types
**Rationale:** Zero dependencies, establishes the vocabulary all subsequent phases use. Fast to build, high confidence. Enables test-first development.
**Delivers:** Interfaces (`IApplicationState`, `IOverlay`, `IPlugin`, `IStateUI`), `ServiceProvider` concept, `StandaloneServiceProvider`, `StateMachine<TStateId>`, `AppSubsystem`/`StateSubsystem` base classes, capability tag constants.
**Addresses:** Table stakes (state machine interface, plugin interface, subsystem scoping types, headless service provider).
**Avoids:** No pitfalls to navigate -- pure new code with no v1 entanglement.

### Phase 2: Subsystem Infrastructure
**Rationale:** `SubsystemRegistry` and `EngineContext` are the spine of v2. Everything interacts with them. Must be solid and tested before any consumer work.
**Delivers:** `SubsystemRegistry<TBase>` with `DependsOn` + topological sort + capability activation, `EngineContext` v2 with typed subsystem access and scoped registries.
**Addresses:** Table stakes (dependency-ordered init/shutdown, abstract-type resolution, central service locator), differentiator (capability-gated activation).
**Avoids:** Pitfall #2 (init-order dependencies) by implementing sort before decomposition. Pitfall #12 (EngineContext god object) by constraining the interface from day one.

### Phase 3: Plugin Composition and AppBuilder
**Rationale:** `AppBuilder` is the plugin's interaction surface. Must exist before concrete states or overlays can be registered. Depends on Tier 0 interfaces and Tier 1 registrars.
**Delivers:** `AppBuilder` with typed registrar store, `AppDescriptor` read-only snapshot, state/overlay/subsystem registration API, `Finalise()` with comprehensive validation.
**Addresses:** Table stakes (plugin as sole extension unit, dependency declaration, plugin groups).
**Avoids:** Pitfall #7 (non-deterministic build order) by accumulate-then-validate pattern in `Finalise()`. Pitfall #11 (capability typos) by validating all capability tags against known providers.

### Phase 4: Simulation and Orchestration
**Rationale:** The three primary orchestration types -- `Simulation`, `ApplicationStateMachine`, `OverlayStack` -- consume the infrastructure from Phases 1-3 and produce the framework that concrete states consume.
**Delivers:** `Simulation` (headless-capable, ServiceProvider-based), `ApplicationStateMachine` (flat + push/pop with full lifecycle), `OverlayStack` (ordered, capability-filtered, event-consuming).
**Addresses:** Differentiators (push/pop negotiation, ServiceProvider concept, capability-gated overlays).
**Avoids:** Pitfall #1 (re-entrancy) by guard flag + deferred-only transitions. Pitfall #5 (empty capabilities) by batched capability transitions. Pitfall #16 (stack depth leak) by max stack depth assertion.

### Phase 5: Concrete States and Engine Decomposition
**Rationale:** First integration of all v2 systems into real use. Proves the architecture end-to-end. EngineRuntime decomposition is the riskiest step and needs all infrastructure in place.
**Delivers:** `GameplayState` wrapping Simulation, `EditorState` stub, `FpsOverlay`, Window/Input/Time/Renderer as individual `AppSubsystem` instances.
**Addresses:** Table stakes (GameplayState proves the whole system), differentiators (IStateUI lifecycle, sub-state machines).
**Avoids:** Pitfall #4 (circular scope deps) by mapping every EngineRuntime member to its correct v2 scope before starting. Pitfall #10 (premature LayerStack removal) by keeping v1 running until v2 overlay is functional.

### Phase 6: Application Rewrite and Integration
**Rationale:** Everything v2 must exist and be tested before the Application can be rewritten. The frame loop switchover must be atomic.
**Delivers:** Rewritten `Application::Initialise()` and `Application::Loop()` using v2 architecture. Updated Journey sandbox using plugin composition and AppBuilder.
**Addresses:** Full end-to-end migration. Journey runs on v2 architecture.
**Avoids:** Pitfall #6 (non-atomic switchover) by writing the entire v2 loop and swapping in one commit. Pitfall #9 (stale test fixtures) by updating all tests in the same commits as API changes.

### Phase 7: Cleanup
**Rationale:** Only safe after all v2 paths are tested and Journey runs. Removes all dead v1 code.
**Delivers:** Removal of LayerStack, Layer, PluginExport/Loader, old EngineRuntime, old EngineContext struct, Game class, DLL plugin system, backward-compat aliases.
**Addresses:** Clean codebase with no dead code paths.

### Phase Ordering Rationale

- **Dependency-driven:** The strict DAG (foundation -> infrastructure -> orchestration -> concrete) means skipping or reordering phases creates scaffolding or compilation failures.
- **Risk-loaded early:** Subsystem infrastructure (Phase 2) and the capability system are the highest-risk novel components. Building them early maximises time for iteration.
- **Integration loaded late:** The frame loop switchover (Phase 6) and cleanup (Phase 7) are inherently late-stage -- they depend on everything else being proven.
- **Tests pass at every phase boundary:** Each phase produces testable output. v1 tests remain green until Phase 6 rewrites them.

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 4 (Simulation):** Simulation/Game decomposition touches flecs world ownership and the static accessor problem. Needs careful design of Game -> Simulation transition.
- **Phase 5 (Engine Decomposition):** EngineRuntime's `SceneRenderExtractor` straddles app and state scopes. Must resolve whether it's state-scoped or app-scoped with late-binding.

Phases with standard patterns (skip research):
- **Phase 1 (Foundation):** Pure interfaces and concepts -- well-documented C++23 patterns, proven in existing engine code.
- **Phase 2 (Subsystem Infra):** Topological sort already implemented in `SystemRegistrar`. Extension, not invention.
- **Phase 3 (AppBuilder):** Type-indexed heterogeneous containers are a well-established C++ pattern.
- **Phase 7 (Cleanup):** Mechanical removal with compilation verification.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All patterns are pure C++23, no external deps. Key features (`Result<T>`, concepts, `GameplayTag`) already proven in codebase. |
| Features | HIGH | Grounded in analysis of Unreal, Bevy, Godot, O3DE, Fyrox + existing v2 planning docs. Table stakes are well-established. |
| Architecture | HIGH | Derived from actual codebase inspection (~7500 LOC measured). Dependency DAG is concrete, not theoretical. Tiered migration validated against real coupling points. |
| Pitfalls | HIGH | All 16 pitfalls reference specific files, line numbers, or documented design decisions. No speculative risks -- every pitfall is grounded in actual code analysis. |

**Overall confidence:** HIGH

### Gaps to Address

- **`std::flat_set`/`std::flat_map` availability:** Must verify these compile on the project's Clang/libc++ version. Trivial `std::vector` fallback exists, but should be confirmed early (Phase 1).
- **Deducing `this` codegen quality on Clang:** Used for CRTP-free mixins in STACK.md. Verify Clang support quality before relying on it; explicit template parameter fallback is trivial.
- **`SceneRenderExtractor` scope:** Research identified it straddles app/state lifetimes. Must be resolved during Phase 5 planning -- either make it a `StateSubsystem` with `Capability::Simulation + Capability::Rendering`, or keep it app-scoped with explicit `AttachWorld()`/`DetachWorld()`.
- **Single-frame stepping for tests:** FEATURES.md identifies `App::Update()` single-frame stepping as expected but not yet designed in v2. Needed for test ergonomics.
- **Config validation at load time:** Noted as table stakes but not yet designed. Per-plugin TOML loading with error messages for authors can be deferred but should be tracked.

## Conflicts and Tensions

- **EngineContext scope vs. god object risk:** Research prescribes EngineContext as the central access mechanism (replacing globals) while simultaneously warning it could become a new monolith (Pitfall #12). Resolution: constrain to generic `GetSubsystem<T>()` only -- no domain-specific convenience methods. Code review as a gate.
- **Static accessor survival vs. DI purity:** The `ServiceProvider` concept aims for clean dependency injection, but flecs callbacks force `StateSubsystems::Get<T>()` to persist as a bounded static escape hatch. Resolution: accept the dual-access pattern for this milestone. The static is renamed, scoped, and documented -- not eliminated.
- **Capability system innovation vs. migration risk:** The capability-gated activation is v2's strongest differentiator but also its most novel component. Building it early (Phase 2-3) maximises iteration time but means the foundation is newer, less battle-tested code. Resolution: build it early, test exhaustively, and the standard patterns in subsequent phases validate it through use.

## Sources

### Primary (HIGH confidence)
- Wayfinder codebase: `engine/wayfinder/src/app/`, `engine/wayfinder/src/gameplay/`, `engine/wayfinder/src/plugins/`, `engine/wayfinder/src/scene/`, `tests/`
- Wayfinder planning docs: `docs/plans/application_architecture_v2.md`, `docs/plans/application_migration_v2.md`, `docs/plans/game_framework.md`
- Codebase analysis: `.planning/codebase/ARCHITECTURE.md`, `.planning/codebase/CONCERNS.md`
- C++23 Standard (ISO/IEC 14882:2024) -- all language-level patterns

### Secondary (MEDIUM confidence)
- Unreal Engine architecture: GameMode/GameState lifecycle, USubsystem hierarchy, Lyra gameplay tag patterns
- Bevy 0.18: `bevy_app::App` API, Plugin trait, States system (docs.rs/bevy)
- Godot 4: SceneTree, Node lifecycle, EditorPlugin
- O3DE: Gem system, AZ::Module, AZ::Interface<T>

### Tertiary (LOW confidence)
- Fyrox Plugin trait: game entry point pattern -- less widely documented

---
*Research completed: 2026-04-03*
*Ready for roadmap: yes*
