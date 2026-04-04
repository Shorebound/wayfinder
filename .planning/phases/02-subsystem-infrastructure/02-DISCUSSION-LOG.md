# Phase 2: Subsystem Infrastructure - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md -- this log preserves the alternatives considered.

**Date:** 2026-04-04
**Phase:** 02-subsystem-infrastructure
**Areas discussed:** Registry API shape, EngineContext v2 scope, Capability activation flow, Error handling and validation

---

## Registry API Shape

### Finalise-then-Initialise split

| Option | Description | Selected |
|--------|-------------|----------|
| Finalise-then-Initialise (internal) | Register builds descriptors, Finalise validates graph + freezes, Initialise creates in order. Fail before any subsystem is constructed. Matches StateMachine and O3DE patterns. | ✓ |
| Combined validate+create | Single Initialise validates graph then creates. Simpler API. Failure means partial rollback. | |
| No validation phase | Register and create inline. Errors surface at creation time only. | |

**User's choice:** Finalise-then-Initialise, but only after understanding the DX impact. Key insight: developer never calls Finalise or Initialise directly. They register in Build(), call Run(), and the engine handles validation and creation internally.
**Notes:** User asked for real-world engine comparisons (Unreal, O3DE, Godot). O3DE's Gem system uses a similar descriptor + topological sort pattern. Unreal has no explicit dependency ordering (convention-based). Decision driven by the "fail before creating anything" benefit.

### Dependency declaration style

| Option | Description | Selected |
|--------|-------------|----------|
| Descriptor with designated initialisers | DependsOn = { typeid(Window) }. All metadata in one block. Consistent with StateMachine pattern. | ✓ |
| Fluent builder API | registry.Register<T>().DependsOn<Window>(). More autocomplete-friendly. | |
| Virtual method on subsystem | Subsystem declares GetDependencies(). O3DE style. | |

**User's choice:** Descriptor. Noted that fluent API is cleaner (DependsOn<T>() vs typeid(T)) but descriptor matches project style better. Fluent noted as future direction if a DX cohesion pass happens.
**Notes:** User asked for side-by-side code examples at simple, medium, and complex scale to compare readability.

### Abstract-type resolution

| Option | Description | Selected |
|--------|-------------|----------|
| Two-template-param registration | Register<SDL3Window, Window>. Queryable by either type. Duplicates caught at Finalise. | ✓ |
| Self-declaring abstract type | Subsystem declares `using AbstractType = Window;`. Auto-detected. | |
| Separate RegisterAbstract call | Explicit second registration. More ceremony. | |

**User's choice:** Two-template-param. Clean, explicit.

### Static accessor replacement

| Option | Description | Selected |
|--------|-------------|----------|
| ECS singleton component | EngineContext stored as flecs singleton. Set on state enter, removed on exit. Thread-safe. | ✓ |
| Lambda capture at registration | Capture EngineContext& when registering flecs systems. | |
| Keep static accessor (renamed) | Rename GameSubsystems to StateSubsystems. Same Bind/Unbind. | |

**User's choice:** ECS singleton. User initiated this by asking about thread safety concerns with the static accessor. Agreed that eliminating global mutable state is correct.
**Notes:** User confirmed understanding that EngineContext flows from Application -> states -> ECS world singleton. No globals needed at any level.

### Subsystem scope enforcement

| Option | Description | Selected |
|--------|-------------|----------|
| Template parameter for scope | SubsystemRegistry<AppSubsystem> and SubsystemRegistry<StateSubsystem>. Compile-time enforcement. | ✓ |
| Runtime scope field | One SubsystemRegistry type, scope checked at registration. | |

**User's choice:** Template parameter. Compile-time over runtime.

### Subsystem Initialise signature

| Option | Description | Selected |
|--------|-------------|----------|
| Initialise(EngineContext&) | Full cross-scope access. State subsystems can reach app subsystems. | ✓ |
| Initialise(SubsystemRegistry&) | Own-scope only. Cross-scope needs separate mechanism. | |
| Constructor injection | Dependencies injected via constructor. No query needed. | |

**User's choice:** EngineContext&. After seeing real-world examples where state subsystems need app subsystems (PhysicsSubsystem needing TimeSubsystem from app scope).

### Get<T>() for gated-off subsystems

| Option | Description | Selected |
|--------|-------------|----------|
| Assert Get + nullable TryGet | Get<T>() asserts (declared dependency must exist). TryGet<T>() returns nullable. Two-tier from Phase 1. | ✓ |
| Both nullable | Both return nullable. Caller always null-checks. | |

**User's choice:** Two-tier. User confirmed that TryGet covers the nullable case, making a nullable Get redundant.

### Shutdown order

| Option | Description | Selected |
|--------|-------------|----------|
| Reverse topological of init order | Mirrors init order. If A depends on B, B shuts down after A. Automatic from dependency graph. | ✓ |
| Reverse registration order (current) | Simple but ignores dependency relationships. | |

**User's choice:** Reverse topological. Straightforward.

---

## EngineContext v2 Scope

### Phase 2 API surface

| Option | Description | Selected |
|--------|-------------|----------|
| Full interface with stubs | Full method signatures. Phase 4 features assert if called. Locks contract early. | ✓ |
| Subsystem access + project + stop only | Minimal. Transition/overlay methods added in Phase 4. | |
| Registry owner only | No lifecycle methods at all. | |

**User's choice:** Full interface with stubs. User asked whether there are issues with this approach. Confirmed: no real issues since methods won't be called until Phase 4+ code exists.

### Ownership model

| Option | Description | Selected |
|--------|-------------|----------|
| Facade (Application owns) | Application owns registries as value members. EngineContext is non-owning raw-pointer facade. Name matches semantics. | ✓ |
| EngineContext owns | EngineContext owns everything. Simpler but name is misleading. | |

**User's choice:** Facade. User raised the naming concern ("EngineContext owning feels like a misnomer"). Explored three options (EngineContext owns, Application owns with facade, rename). Settled on Application owns because "context" should mean "view into state, not owner of state."
**Notes:** User asked about raw vs weak pointers. Confirmed raw pointers correct: clear non-owning semantics, no shared ownership, Application outlives EngineContext. Registries are concrete types (not polymorphic), stored as value members on Application.

### Old EngineContext disposition

| Option | Description | Selected |
|--------|-------------|----------|
| Delete and replace in-place | Delete old struct. Replace with new class. Compile errors guide fixes. | ✓ |
| Keep alongside temporarily | Legacy coexistence. | |

**User's choice:** Delete and replace. User emphasised: "We do not care about migration or legacy stuff. There are no consumers."

### Read/write access control

| Option | Description | Selected |
|--------|-------------|----------|
| Single type, const for reads | One type. Reads are const, writes are non-const. Split later if threading demands it. | ✓ |
| Split read/write types | EngineView (read) + EngineContext (read+write) inheritance. | |

**User's choice:** Single type. After seeing real-world engine comparisons (Unreal: single UWorld; Bevy: explicit System params). Deferred command pattern provides runtime safety. Const correctness provides compile-time guidance.

### Testability

| Option | Description | Selected |
|--------|-------------|----------|
| Directly constructible with partial pointers | No Application needed. Null pointers for unneeded features. | ✓ |
| Tests bypass EngineContext entirely | Use StandaloneServiceProvider. | |

**User's choice:** Directly constructible. Enables unit testing subsystem init without full application bootstrap.

### Access model

| Option | Description | Selected |
|--------|-------------|----------|
| Single EngineContext instance, passed everywhere | Same instance throughout. All lifecycle methods receive it. | ✓ |
| Different access types per level | States get EngineContext. Subsystems get SubsystemRegistry. | |

**User's choice:** Single instance. Simplest model.

---

## Capability Activation Flow

### Effective set computation

| Option | Description | Selected |
|--------|-------------|----------|
| Union model | App caps (hardware) + state caps (domain) = effective set. Headless-friendly. | ✓ |
| Additive state-only | Functionally same as union. | |
| Intersection | Both sources must declare a cap. Verbose, breaks headless. | |

**User's choice:** Union. After requesting a plain explanation of the entire capability system and what game developers actually see. Key insight: developers declare requirements on subsystems and capabilities on states; engine handles matching.
**Notes:** User asked for a complete walkthrough of what the system does and what's exposed to game developers. The plain explanation was instrumental.

### Transition timing

| Option | Description | Selected |
|--------|-------------|----------|
| Batched (no intermediate) | Compute diff, swap atomically. Shared caps stay active. No teardown+reinit. | ✓ |
| Incremental | Remove old, add new. Brief intermediate. Risk of unnecessary teardown. | |

**User's choice:** Batched. Dealbreaker for incremental was teardown+reinit for shared capabilities.

### Check semantics

| Option | Description | Selected |
|--------|-------------|----------|
| HasAll only (extend later) | Every required cap must be present. Add HasAny later if real use case emerges. | ✓ |
| Both HasAll + HasAny from day one | CapabilityMatch enum. | |

**User's choice:** HasAll only. User asked about user-definable matching. Agreed HasAny is niche and non-breaking to add later.

---

## Error Handling and Validation

### Cycle detection reporting

| Option | Description | Selected |
|--------|-------------|----------|
| Full cycle path in error message | "Cycle detected: Renderer -> Window -> Renderer". Returned from Finalise(). | ✓ |
| Minimal error message | "Cycle detected in subsystem graph". | |

**User's choice:** Full path. Straightforward.

### Init failure strategy

| Option | Description | Selected |
|--------|-------------|----------|
| Fail-fast with RAII cleanup | First failure aborts. Already-created subsystems shut down in reverse order. | ✓ |
| Collect all errors | Continue past failures, collect all errors, then shut everything down. | |

**User's choice:** Fail-fast. User asked about benefits of collect-all. Acknowledged the "see all errors at once" benefit but agreed it doesn't justify the cascade complexity (later subsystems depending on failed ones).

### Optional subsystems

| Option | Description | Selected |
|--------|-------------|----------|
| No optional flag | Capabilities handle optionality. Created subsystems must succeed. Add Optional later if needed. | ✓ |
| Optional flag from day one | Optional descriptor field for non-fatal failures. | |

**User's choice:** No optional flag. User asked for detailed code examples. Key insight: capabilities ARE the optionality mechanism. Gated-off = never created. Created = must succeed.

### Missing dependency detection

| Option | Description | Selected |
|--------|-------------|----------|
| Finalise primary, assert as safety net | Finalise catches structural errors. Get<T>() assert is safety net only. | ✓ |
| Assert-only at runtime | No pre-creation validation beyond cycles. | |

**User's choice:** Finalise primary. Catches unregistered deps, incompatible cap requirements, duplicates before any subsystem is constructed.

---

## Agent's Discretion

- Topological sort algorithm choice (Kahn's vs DFS-based)
- Internal storage data structures for SubsystemRegistry
- EngineContextRef struct design for flecs singleton
- Error message formatting details
- Whether Deps<A,B>() helper is worth adding

## Deferred Ideas

None -- discussion stayed within phase scope.
