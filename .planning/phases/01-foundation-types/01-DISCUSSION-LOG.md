# Phase 1: Foundation Types - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md -- this log preserves the alternatives considered.

**Date:** 2026-04-03
**Phase:** 01-Foundation Types
**Areas discussed:** Interface lifecycle design, ServiceProvider contract, StateMachine generics, File organisation, Capability system

---

## Interface Lifecycle Design

### Lifecycle return types

| Option | Description | Selected |
|--------|-------------|----------|
| Result<void> | All lifecycle methods return Result<void> for error propagation | |
| void + event bus | Void returns, errors through event bus | |
| Mixed | Result<void> for OnEnter/OnExit, void for per-frame methods | ✓ |

**User's choice:** Mixed
**Notes:** OnEnter/OnExit can fail meaningfully (resource loading, validation). Per-frame methods (OnUpdate, OnRender, OnEvent, OnSuspend, OnResume) should not fail.

### Dispatch mechanism

| Option | Description | Selected |
|--------|-------------|----------|
| Pure virtual (abstract base) | Runtime polymorphism, matches existing patterns | ✓ |
| CRTP | Static dispatch, but requires type erasure for state machine anyway | |
| Deducing this | Useful inside concrete implementations, doesn't replace virtual | |

**User's choice:** Pure virtual
**Notes:** User requested detailed comparison of all three options. CRTP adds complexity for zero gain (state machine needs runtime polymorphism). Deducing this solves forwarding, not polymorphism. User agreed pure virtual is the right call after seeing the code comparison.

### Context parameter

| Option | Description | Selected |
|--------|-------------|----------|
| EngineContext& on methods | States constructible without engine. Context threaded through calls. | ✓ |
| Stored reference from constructor | States receive context at construction. | |
| ServiceProvider concept parameter | Template-based, loses virtual dispatch. | |

**User's choice:** EngineContext& on methods
**Notes:** User requested pros/cons analysis. Best testability (construct state, call with test context), no coupling at construction time. ServiceProvider used for Simulation separately.

### Event delivery

| Option | Description | Selected |
|--------|-------------|----------|
| EventQueue& (batch) | Batch processing via Drain<T>(). One virtual call per frame. | ✓ |
| SDL_Event directly | Individual events, couples to SDL | |
| Typed event visitor | std::variant per event, type-safe but 50x more virtual calls | |

**User's choice:** EventQueue& (batch)
**Notes:** User was considering option 1 or 3. Requested overhead comparison. Option 3 adds ~50 virtual calls per frame vs 1 for batch. EventQueue already handles per-type batching well. User confirmed option 1 after seeing the code comparison.

---

## ServiceProvider Contract

### Missing service behaviour

| Option | Description | Selected |
|--------|-------------|----------|
| Get asserts, TryGet returns null | Two-tier API with clear intent | ✓ |
| All fallible | Both return Result/optional | |
| Get throws | Violates hot paths never throw | |

**User's choice:** Get asserts, TryGet returns null
**Notes:** User asked "do we even want Get?" -- explored whether TryGet-only would suffice. Showed that Get eliminates null check + pointer dereference at every callsite where dependency is known to exist. User agreed two-tier API with clear intent justifies both methods.

### Simulation storage

| Option | Description | Selected |
|--------|-------------|----------|
| Template parameter | Simulation<TProvider>, zero overhead but viral template spread | |
| Type-erased wrapper (ServiceLocator) | Simulation stays concrete. One virtual per Get<T>(). | ✓ |
| Separate classes | Worse option 2 without polymorphism | |

**User's choice:** Type-erased wrapper (ServiceLocator)
**Notes:** User asked what problem ServiceProvider solves in the first place. Showed three concrete scenarios: headless tests (no window/GPU), CLI tools (waypoint), future editor (multiple sims). User agreed the decoupling justifies the abstraction.

### StandaloneServiceProvider internals

| Option | Description | Selected |
|--------|-------------|----------|
| type_index map + void* | Industry standard, type_index key guarantees type safety | ✓ |
| Static tuple | Fastest but fixed service set per instantiation | |
| std::any map | Redundant safety, std::any overhead | |

**User's choice:** type_index map + void*
**Notes:** User requested all three shown in practice. Each was demonstrated with real code. User selected option 1 after seeing the comparison.

---

## StateMachine Generics

### Callback storage

| Option | Description | Selected |
|--------|-------------|----------|
| std::function | Lambdas, bind_front, any callable. Transition-frequency path. | ✓ |
| Function pointer | C-style void* callbacks. Zero overhead but poor DX. | |
| Virtual on state descriptor | IStateDescriptor per state. Overkill for lightweight sub-states. | |

**User's choice:** std::function
**Notes:** User asked about DX for both small and large games. Showed Pang clone (lambdas in one file) vs Ratchet & Clank scale (class-per-state with bind_front). Demonstrated optional convenience overload for auto-binding. User confirmed after seeing both scales.

### Push/pop location

| Option | Description | Selected |
|--------|-------------|----------|
| Flat only in generic, push/pop in AppStateMachine | Keeps generic template simple | ✓ |
| Both in generic template | More capable but unnecessary complexity for sub-states | |

**User's choice:** Separate types (flat generic + specialised app)
**Notes:** User asked about two-interface composition (IStateMachine + IModalStack). Explored: IModalStack has no standalone value -- push/pop is inherently coupled to state identity. No consumer needs just one half. Single cohesive type is correct.

### Registration pattern

| Option | Description | Selected |
|--------|-------------|----------|
| Descriptor-based, finalise-then-run | Validate graph at startup, freeze machine | ✓ |
| Dynamic registration | States added/removed at runtime | |
| Finalise + unlock escape hatch | Option 1 with future dynamic support | |

**User's choice:** Finalise-then-run with @todo for escape hatch
**Notes:** User asked if validation was important and what we'd lose without it. Showed: typo catching at startup, unreachable state detection, and that validation actually improves runtime performance (pre-validated transitions are O(1) vs per-transition checks). User also asked about string-based keying alternatives -- showed enum class, handles, and confirmed generic TStateId template param covers all strategies.

### State keying

| Option | Description | Selected |
|--------|-------------|----------|
| Generic TStateId template param | Callers choose InternedString, enum class, handles, etc. | ✓ |
| InternedString only | Simpler but less flexible | |

**User's choice:** Generic TStateId template param
**Notes:** No additional discussion needed.

---

## File Organisation

### Type placement

| Option | Description | Selected |
|--------|-------------|----------|
| In-place (existing dirs) | New types in permanent domain dirs. I-prefix signals v2. | ✓ |
| Staging dir (v2/) | Temporary during migration. Move files twice. | |
| New subdirectories | Fresh dir names matching v2 concepts. | |

**User's choice:** In-place
**Notes:** User asked if staging dir had real benefits. Answer: no -- I-prefix naming already signals v2, staging means moving files twice, includes get messy.

### IPlugin rename

| Option | Description | Selected |
|--------|-------------|----------|
| Rename file + class | Clean break, old references fail to compile | ✓ |
| New file alongside old | Both exist during migration | |

**User's choice:** Rename file + class
**Notes:** No additional discussion.

### Headers vs modules

| Option | Description | Selected |
|--------|-------------|----------|
| Headers (#pragma once) | Proven, all tooling supports it | |
| Modules (.ixx) | Faster builds but tooling friction | |
| Headers now, module-ready | Headers with 1:1 module mapping structure | ✓ |

**User's choice:** Headers now, module-ready
**Notes:** User asked for real-world examples of where modules work vs don't. Showed: pure C++ types (core, interfaces) are good candidates; anything touching C libraries (SDL3, flecs, Jolt) stays as headers due to global module fragment friction and tooling gaps.

### File granularity

| Option | Description | Selected |
|--------|-------------|----------|
| One file per interface/type | Clear, grep-friendly | ✓ |
| Grouped by domain | Fewer files, single include | |

**User's choice:** One file per interface/type
**Notes:** No additional discussion.

---

## Capability System

### CapabilitySet type

| Option | Description | Selected |
|--------|-------------|----------|
| Use GameplayTagContainer directly with alias | using CapabilitySet = GameplayTagContainer. Add AddTags() merge method. | ✓ |
| Separate CapabilitySet class | Dedicated type with union/intersection ops | |

**User's choice:** GameplayTagContainer with type alias
**Notes:** User raised this proactively -- noticed the v2 plan mentions capability sets but GameplayTagContainer already provides HasAll/HasAny/AddTag/RemoveTag. Only need AddTags(const GameplayTagContainer&) for merge. Alias for domain clarity.

---

## Agent's Discretion

- Internal file naming within domain dirs
- Exact method signatures for IOverlay and IStateUI
- Whether to add the convenience AddState(id, stateObject) overload in Phase 1 or defer

## Deferred Ideas

None -- discussion stayed within phase scope.
