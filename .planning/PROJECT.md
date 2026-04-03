# Wayfinder - Application Architecture v2 Migration

## What This Is

A full architectural migration of the Wayfinder game engine from its current monolithic application shell (EngineRuntime + LayerStack + Game) to the v2 plugin-composed architecture (ApplicationStateMachine + capability-gated subsystems + OverlayStack + Simulation). This covers both the app shell layer and the game framework layer in a single milestone.

## Core Value

The engine's application architecture is clean, extensible, and compositional - so that every future feature (asset handling, rendering improvements, editor, audio, networking) plugs in through well-defined extension points rather than fighting a monolithic runtime.

## Requirements

### Validated

- ✓ Plugin/Registrar system with topological ordering - existing
- ✓ SubsystemCollection with predicate filtering and ShouldCreate() - existing
- ✓ EventQueue with typed event dispatch - existing
- ✓ GameplayTag system with InternedString-backed hierarchical tags - existing
- ✓ ECS integration via flecs with SystemRegistrar, StateRegistrar, TagRegistrar - existing
- ✓ GameStateMachine with ActiveGameState singleton and RunCondition helpers - existing
- ✓ ProjectDescriptor and EngineConfig loading - existing
- ✓ Render graph pipeline with RenderFeature/RenderPass architecture - existing

### Active

- [ ] IApplicationState interface with full lifecycle (enter/exit/suspend/resume/update/render/event)
- [ ] ApplicationStateMachine with hybrid flat + push/pop model and transition validation
- [ ] IOverlay interface and OverlayStack with capability-gated activation
- [ ] AppSubsystem and StateSubsystem scoped base classes
- [ ] SubsystemRegistry with dependency ordering and capability-based activation
- [ ] EngineContext as central service-access mechanism (replaces globals and EngineRuntime)
- [ ] Capability system using GameplayTag for activation control
- [ ] AppBuilder replacing PluginRegistry with typed registrar store
- [ ] IPlugin interface replacing Plugin (with Build-only pattern, no OnStartup/OnShutdown)
- [ ] Simulation class replacing Game (flecs world + scene management, ServiceProvider access)
- [ ] ServiceProvider concept and EngineContextServiceProvider adapter
- [ ] StandaloneServiceProvider for headless tests and tools
- [ ] GameplayState wrapping Simulation into IApplicationState lifecycle
- [ ] EditorState stub proving the IApplicationState pattern
- [ ] StateMachine<TStateId> generic template for per-state sub-state machines
- [ ] IStateUI interface for plugin-injected per-state UI
- [ ] Per-plugin configuration replacing monolithic EngineConfig
- [ ] Canvas-based render submission (SceneCanvas, UICanvas, DebugCanvas)
- [ ] FpsOverlay rewritten from FpsOverlayLayer
- [ ] DLL plugin system removal (PluginExport.h, PluginLoader.h, CreateGamePlugin)
- [ ] LayerStack removal
- [ ] EngineRuntime decomposition into individual AppSubsystems
- [ ] Journey sandbox updated to use new architecture
- [ ] Existing tests updated or rewritten for new architecture

### Out of Scope

- Full editor implementation - only EditorState stub proving the pattern
- UI toolkit / widget system - IStateUI interface only, no concrete toolkit
- Hot-reload / mod support - noted as @todo in v2 docs, not this milestone
- Multi-threaded frame extraction - architecture designed for it, not implemented
- Asset pipeline overhaul - migration-forced changes only, issues filed for the rest
- Networking, audio subsystems - future plugins using the new architecture
- Visual UI designer tooling - @todo in v2 docs
- Tag-gated widget activation - @todo in v2 docs

## Context

Wayfinder is a C++23 game engine targeting "fantasy console" aesthetics (sixth-gen with modern compute). It uses SDL3, Flecs ECS, Slang shaders, Jolt Physics, and a render graph pipeline with SDL_GPU backend (Vulkan/D3D12/Metal).

The current v1 architecture has a monolithic `EngineRuntime` that owns all platform and rendering services, a `LayerStack` for frame lifecycle, and a `Game` class that owns the flecs world, subsystem collection, and game state machine. This works but creates tight coupling, makes extension points unclear, and will become increasingly painful as features are added.

Three planning documents define the target architecture in detail:
- `docs/plans/application_architecture_v2.md` - the app shell (ApplicationStateMachine, EngineContext, subsystem scoping, overlays, plugins, rendering integration, lifecycle timeline)
- `docs/plans/application_migration_v2.md` - exact rename/keep/remove/add transition tables
- `docs/plans/game_framework.md` - Simulation, GameplayState, EditorState, ServiceProvider, sub-state machines, IStateUI

The codebase is ~7500 LOC engine, ~3000 LOC tests, with 37 test files across 4 test executables.

## Constraints

- **Build system**: CMake 4.0+ with Ninja Multi-Config + Clang. Source files listed explicitly in CMakeLists.txt.
- **Dependencies**: All existing (SDL3, Flecs, Jolt, etc.) are Shorebound forks managed via CPM. No new external dependencies for this migration.
- **Testing**: doctest, headless only (Null backend). Journey can break temporarily during structural phases as long as tests pass at phase boundaries.
- **Continuity**: No need to maintain backward compatibility - sole developer, greenfield engine.
- **Opportunistic cleanup**: Migration-forced changes to asset/rendering code are in scope. Larger cleanups get TODOs or GitHub issues for future work.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Both layers in one milestone | App shell and game framework are tightly coupled in the plans; splitting them would mean half-migrated code living longer | -- Pending |
| DLL plugin system removed entirely | No legacy users, v2 has game own main(), dead code confuses contributors | -- Pending |
| EditorState as stub only | Proves the IApplicationState pattern without building a real editor | -- Pending |
| Full capability system | GameplayTag-based activation for subsystems, overlays, and render features - not just declaration | -- Pending |
| Journey can break temporarily | Allows structural changes without maintaining a working sandbox at every commit | -- Pending |
| Opportunistic cleanup with issue tracking | Touch asset/rendering only where migration forces it; file issues for larger cleanup | -- Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? -> Move to Out of Scope with reason
2. Requirements validated? -> Move to Validated with phase reference
3. New requirements emerged? -> Add to Active
4. Decisions to log? -> Add to Key Decisions
5. "What This Is" still accurate? -> Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check - still the right priority?
3. Audit Out of Scope - reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-04-03 after initialization*
