# Phase 2: Subsystem Infrastructure - Research

**Researched:** 2026-04-04
**Domain:** Subsystem registry design, dependency-ordered initialisation, capability-gated activation, service locator facade
**Confidence:** HIGH

## Summary

Phase 2 evolves the existing `SubsystemCollection<TBase>` into `SubsystemRegistry<TBase>` with topological dependency ordering, capability-gated activation, abstract-type resolution, and `Result<void>` error propagation. It also replaces the 27-line `EngineContext` struct with a non-owning facade class providing typed access to both app and state subsystem registries, and removes the static `GameSubsystems` accessor in favour of an ECS singleton component.

The codebase already has two working implementations of Kahn's topological sort (SystemRegistrar and RenderGraph), a proven type_index-keyed registry pattern (SubsystemCollection), and the Tag/TagContainer capability infrastructure from Phase 1. The subsystem collection API shape (Register/Get/Initialise/Shutdown) is preserved and enhanced. The primary engineering is: (1) descriptor-based registration with DependsOn, (2) dependency graph validation + cycle detection at Finalise(), (3) capability set computation and filtering at activation time, (4) dual type_index registration for abstract-type resolution, and (5) EngineContext v2 as a thin facade over the registries.

**Primary recommendation:** Evolve SubsystemCollection in-place into SubsystemRegistry. Use Kahn's algorithm for topological sort (proven twice in the codebase). Store the sorted init order as a vector of indices for O(1) reverse-order shutdown. Abstract types use a secondary type_index -> primary type_index redirect map. EngineContext is a struct-like class holding raw pointers to Application's members, constructed partially for headless tests.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Finalise-then-Initialise split as an internal-only detail. Developers register subsystems in their plugin's `Build()` method. `AppBuilder::Finalise()` calls `SubsystemRegistry::Finalise()` internally (validates dependency graph, detects cycles, freezes registration). Then `SubsystemRegistry::Initialise()` creates instances in topological order. The game developer never calls either method directly.
- **D-02:** Descriptor-based registration using designated initialisers. All metadata (RequiredCapabilities, DependsOn) in a single aggregate block. Consistent with StateMachine descriptor pattern from Phase 1.
- **D-03:** Two-template-parameter abstract type resolution: `Register<SDL3Window, Window>({...})`. Queryable by either concrete type (`SDL3Window`) or abstract type (`Window`). Duplicate abstract registrations caught at `Finalise()` as a validation error.
- **D-04:** Scope enforced via template parameter: `SubsystemRegistry<AppSubsystem>` and `SubsystemRegistry<StateSubsystem>`. Compile-time scope enforcement.
- **D-05:** Remove static `GameSubsystems` accessor entirely. Replace with ECS singleton component pattern: store an `EngineContextRef` singleton in the flecs world during state entry, remove on state exit. Thread-safe by flecs staging model.
- **D-06:** Subsystem `Initialise(EngineContext&) -> Result<void>`. Receives full EngineContext, not just own-scope registry.
- **D-07:** Shutdown in reverse topological order (mirrors init order).
- **D-08:** Two-tier access: `Get<T>()` asserts and returns `T&`, `TryGet<T>()` returns `T*`.
- **D-09:** Full EngineContext interface declared with assert stubs for Phase 4 features.
- **D-10:** Application owns registries as value members. EngineContext is non-owning facade with raw pointers.
- **D-11:** Old 27-line EngineContext struct replaced in-place (same file, same name).
- **D-12:** Single EngineContext type with mixed const/non-const.
- **D-13:** Directly constructible with partial pointers for headless tests.
- **D-14:** Single EngineContext instance passed by reference to all lifecycle methods.
- **D-15:** Union model for effective capability set (App + State capabilities).
- **D-16:** Batched capability changes on state transition. Compute new set, diff, atomically swap.
- **D-17:** HasAll semantics for RequiredCapabilities check.
- **D-18:** Cycle detection with full cycle path in error message.
- **D-19:** Fail-fast on Initialise() failure with reverse-order shutdown of already-initialised subsystems.
- **D-20:** No optional subsystem flag. Capabilities are the optionality mechanism.
- **D-21:** Finalise() catches structural errors as primary validation.

### Agent's Discretion
- Exact topological sort algorithm (Kahn's vs DFS-based)
- Internal storage data structures for SubsystemRegistry
- Whether `EngineContextRef` is a simple struct with a pointer or slightly richer
- Error message formatting details
- Whether `Deps<A, B>()` type list helper is worth adding

### Deferred Ideas (OUT OF SCOPE)
None - discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| SUB-02 | SubsystemRegistry with DependsOn and topological sort for init/shutdown | Kahn's algorithm pattern from SystemRegistrar; descriptor-based registration; adjacency list + in-degree storage |
| SUB-03 | Capability-based activation for state-scoped subsystems | TagContainer::HasAll() for RequiredCapabilities check; two-source union model; batched swap during transitions |
| SUB-04 | Abstract-type resolution (register concrete under both concrete and abstract type_index) | Secondary redirect map `m_abstractToConcreteIndex`; validated at Finalise() for duplicates |
| SUB-05 | SubsystemRegistry::Initialise returns Result<void> with error propagation | std::expected monadic operations; fail-fast with reverse-order cleanup |
| SUB-06 | EngineContext as central service-access mechanism | Non-owning facade with raw pointers; Get/TryGet forwarding to registries; assert stubs for Phase 4 |
| SUB-07 | StateSubsystems accessor renamed from GameSubsystems, bound/unbound on state transitions | Replaced by flecs singleton `EngineContextRef`; `world.set<EngineContextRef>()` on state entry, `world.remove<EngineContextRef>()` on exit |
| CAP-02 | Two-source model (app-level + state-level capabilities) | Union computed via TagContainer::AddTags(); app caps set at startup, state caps declared per-state |
| CAP-03 | Uniform activation rules for subsystems, overlays, and render features | Single `ShouldActivate(effectiveCaps, requiredCaps)` predicate; shared across all activation-gated types |
| CAP-04 | Empty RequiredCapabilities = always active | TagContainer::IsEmpty() check before HasAll(); empty set is trivially satisfied |
| CAP-05 | Capability set batched during transitions (no intermediate empty state) | Compute new effective set, diff against current, swap atomically, then activate/deactivate |
</phase_requirements>

## Project Constraints (from copilot-instructions.md)

Key directives that the planner MUST honour:

- **C++23** with Clang 22.1 (full C++23 support). std::expected, std::generator, concepts, deducing this, constexpr/consteval all available.
- **British/Australian spelling**: Initialise, Finalise, Serialise, etc.
- **Trailing return types**: `auto Foo(args) -> ReturnType`
- **`and`, `or`, `not`** over `&&`, `||`, `!`
- **West const**: `const T`, never east const
- **`Result<T>`** for recoverable failures (alias for `std::expected<T, Error>`)
- **`[[nodiscard]]`** on functions returning resources or values the caller must inspect
- **RAII everywhere** - wrap every dynamic resource
- **Concepts over SFINAE** - use `requires` clauses or named concepts
- **`WAYFINDER_ASSERT`** macro for engine assertions (logs + aborts, never compiled out)
- **Namespaces**: Engine code in `Wayfinder`; sub-namespaces for domains
- **InternedString** for stable, repeatedly-compared identifiers (used by Tag system)
- **Explicit source file listing** in CMakeLists.txt - new files must be added
- **doctest** for testing; headless only; one file per domain

## Standard Stack

### Core (already in the project)

| Component | Version | Purpose | Why Standard |
|-----------|---------|---------|--------------|
| `std::expected<T, E>` | C++23 / Clang 22 | Result type for Initialise/Finalise return | Already wrapped as `Result<T, TError>` in core/Result.h |
| `std::type_index` | C++ stdlib | Type-erased registry keys | Already used in SubsystemCollection; stable, hashable |
| `std::unordered_map` | C++ stdlib | Type -> instance lookup | Already used in SubsystemCollection |
| `TagContainer` / `Tag` | Engine (gameplay/Tag.h) | Capability sets | HasAll/HasAny/AddTags already implemented |
| `InternedString` | Engine (core/InternedString.h) | O(1) tag equality | Backs the Tag type |
| `WAYFINDER_ASSERT` | Engine (core/Assert.h) | Runtime assertions | Standard engine assert pattern |
| flecs | In project (thirdparty) | ECS singleton for EngineContextRef | `world.set<T>()` / `world.get<T>()` API |

### Supporting (C++23 features to use)

| Feature | Purpose | When to Use |
|---------|---------|-------------|
| `std::derived_from<T, TBase>` | Concept constraint on Register<T> | Compile-time scope enforcement |
| Designated initialisers | SubsystemDescriptor construction | D-02: descriptor-based registration |
| `std::expected::and_then` | Result chaining in init sequence | Error propagation through subsystem chain |
| `auto(x)` / `auto{x}` | Explicit decay-copy in graph algorithms | Avoid dangling references in Kahn's queue |
| `std::ranges::to<Container>()` | Materialise range pipelines | Convert filtered views to vectors |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `std::unordered_map<type_index, T>` | `std::flat_map<type_index, T>` | Better cache locality for small maps (< 50 subsystems). std::flat_map available in Clang 22 via `<flat_map>`, but the STATE.md flags a concern about availability. Recommend staying with `unordered_map` for now; subsystem count is small enough that cache performance is irrelevant. |
| Vector of `type_index` for DependsOn | `Deps<A, B>()` variadic helper | Syntactic sugar. Worth adding - eliminates repeated `typeid()` calls in descriptor and produces a `std::vector<std::type_index>`. |
| `std::generator<T>` for toposort | Materialised vector | Generator is elegant but unnecessary here - the sorted order is stored and reused for shutdown. Materialise once during Finalise(). |

## Architecture Patterns

### Recommended File Structure

```
engine/wayfinder/src/app/
  Subsystem.h              # UPDATE: base classes stay; SubsystemCollection -> SubsystemRegistry
  EngineContext.h           # REPLACE: 27-line struct -> new class (same file)
  EngineContext.cpp         # NEW: implementation for non-template methods
  AppSubsystem.h           # KEEP: unchanged
  StateSubsystem.h         # KEEP: unchanged
  IApplicationState.h      # KEEP: unchanged
```

### Pattern 1: SubsystemRegistry with Descriptor-Based Registration

**What:** SubsystemRegistry<TBase> replaces SubsystemCollection<TBase>. Registration takes a descriptor aggregate instead of bare template + predicate. Supports DependsOn, RequiredCapabilities, and abstract-type aliases.

**How descriptor registration works:**

```cpp
struct SubsystemDescriptor
{
    CapabilitySet RequiredCapabilities;
    std::vector<std::type_index> DependsOn;
};

template<typename TBase>
class SubsystemRegistry
{
public:
    // Single concrete type
    template<std::derived_from<TBase> T>
    void Register(SubsystemDescriptor descriptor = {});

    // Concrete + abstract type alias
    template<std::derived_from<TBase> TConcrete, typename TAbstract>
        requires std::derived_from<TConcrete, TAbstract>
    void Register(SubsystemDescriptor descriptor = {});

    [[nodiscard]] auto Finalise() -> Result<void>;
    [[nodiscard]] auto Initialise(EngineContext& context) -> Result<void>;
    void Shutdown();

    template<typename T> auto Get() -> T&;
    template<typename T> auto TryGet() -> T*;

    template<typename T> auto Get() const -> const T&;
    template<typename T> auto TryGet() const -> const T*;
};
```

**Why this shape:** Matches the existing SubsystemCollection API that consumers know. Descriptor pattern proven by StateMachine<TStateId> from Phase 1. Two-template-param overload for abstract types matches D-03.

### Pattern 2: Kahn's Algorithm for Topological Sort

**What:** Dependency graph built from DependsOn descriptors. Adjacency list + in-degree array. Kahn's algorithm produces the init order; stored as a vector of indices. Shutdown is reverse iteration of the same vector.

**Why Kahn's over DFS:**
1. **Natural cycle detection**: If sorted.size() != total, there's a cycle. DFS needs explicit visited/in-stack tracking.
2. **Deterministic**: When multiple nodes have zero in-degree, a stable tiebreaker (registration order) gives consistent results across runs.
3. **Already used twice**: SystemRegistrar (line 88) and RenderGraph (line 312) both use Kahn's. Consistency matters.
4. **Full cycle path**: When a cycle is detected, all nodes with non-zero in-degree are in the cycle. A secondary DFS/BFS from any such node produces the full path for the error message (D-18).

**Cycle path extraction (enhancement over existing implementations):**

The existing SystemRegistrar just lists cycle members. D-18 requires the full cycle path ("Renderer -> Window -> Renderer"). After Kahn's finds that `sorted.size() < n`:

```cpp
// Find a node still in the cycle (in-degree > 0)
// DFS from that node, following only edges to other in-cycle nodes
// When we revisit a node, we have the cycle path
```

This is a small addition to the existing Kahn's pattern.

### Pattern 3: Abstract-Type Resolution via Redirect Map

**What:** A secondary map `m_abstractIndex` maps `std::type_index(typeid(TAbstract))` -> `std::type_index(typeid(TConcrete))`. Lookups first check the primary map (concrete types), then fall back to the redirect map.

**Why separate redirect instead of dual insertion:** Dual insertion (storing the same pointer under two keys in the primary map) requires careful lifecycle management - shutdown must not process the same subsystem twice. A redirect map keeps ownership unambiguous: one entry in the primary map owns the instance, the redirect just points to it.

**Validation at Finalise():** Iterate the redirect map and check that each concrete target exists in the primary map. Check for duplicate abstract registrations (two concretes mapping to the same abstract).

### Pattern 4: Capability-Gated Activation

**What:** Before creating a subsystem instance, check `effectiveCaps.HasAll(descriptor.RequiredCapabilities)`. If requirements not met, skip creation entirely (no allocation, no failure).

**Two-source union computation:**

```cpp
auto ComputeEffectiveCaps(const CapabilitySet& appCaps, const CapabilitySet& stateCaps) -> CapabilitySet
{
    CapabilitySet effective = appCaps;
    effective.AddTags(stateCaps);
    return effective;
}
```

**Activation flow during state transition:**
1. Compute new effective capability set
2. For each registered state subsystem:
   - If RequiredCapabilities is empty: always create (D-04/CAP-04)
   - If effectiveCaps.HasAll(descriptor.RequiredCapabilities): create (D-17)
   - Otherwise: skip

**Shared subsystems across push/pop:** State subsystems persist across push/pop (Phase 4 concern, documented in architecture spec). Phase 2 provides the mechanism; Phase 4 wires the push/pop lifecycle.

### Pattern 5: EngineContext as Non-Owning Facade

**What:** A class holding raw pointers to Application's member registries, state machine, and overlay stack. Provides typed access methods that forward to the appropriate registry.

```cpp
class EngineContext
{
public:
    // Subsystem access
    template<typename T> auto GetAppSubsystem() const -> T&;
    template<typename T> auto TryGetAppSubsystem() const -> T*;
    template<typename T> auto GetStateSubsystem() const -> T&;
    template<typename T> auto TryGetStateSubsystem() const -> T*;

    // Phase 4 stubs (assert if called)
    template<std::derived_from<IApplicationState> T> void RequestTransition();
    template<std::derived_from<IApplicationState> T> void RequestPush();
    void RequestPop();
    void ActivateOverlay(/* ... */);
    void DeactivateOverlay(/* ... */);

    // Query
    auto GetAppDescriptor() const -> const AppDescriptor&;
    void RequestStop();

private:
    SubsystemRegistry<AppSubsystem>* m_appSubsystems = nullptr;
    SubsystemRegistry<StateSubsystem>* m_stateSubsystems = nullptr;
    // Phase 4 additions (nullptr initially):
    // ApplicationStateMachine* m_stateMachine = nullptr;
    // OverlayStack* m_overlayStack = nullptr;
    // ... etc
};
```

**Headless test construction (D-13):**

```cpp
SubsystemRegistry<AppSubsystem> appSubs;
SubsystemRegistry<StateSubsystem> stateSubs;
EngineContext ctx;
ctx.SetAppSubsystems(&appSubs);
ctx.SetStateSubsystems(&stateSubs);
// state machine, overlay stack, etc. left as nullptr
```

### Pattern 6: ECS Singleton for EngineContext Access (Replacing GameSubsystems)

**What:** A trivial struct component stored as a flecs singleton:

```cpp
struct EngineContextRef
{
    EngineContext* Context = nullptr;
};

// On state entry:
world.set<EngineContextRef>({.Context = &context});

// In flecs systems:
auto* ref = world.get<EngineContextRef>();
auto& physics = ref->Context->GetStateSubsystem<PhysicsSubsystem>();

// On state exit:
world.remove<EngineContextRef>();
```

**Why a simple pointer struct:** Flecs singletons should be trivial components for efficient access. No RAII, no ownership. The architectural guarantee is that Application outlives all states and ECS usage. Thread-safe because flecs controls singleton access through its staging model.

### Anti-Patterns to Avoid

- **Dual ownership through dual map insertion:** Don't store the same `unique_ptr` under two type_index keys. Use a redirect map instead.
- **Init-order dependency on registration order:** After Finalise, init order comes from the topological sort, not registration order. Don't assume registration order equals init order.
- **Capability check at Get<T>() time:** Capabilities are checked at activation time (during Initialise). Get<T>() asserts existence post-init. Don't re-check capabilities on every access.
- **Virtual dispatch for scope enforcement:** Scope is a compile-time template parameter, not a runtime check. `SubsystemRegistry<AppSubsystem>` physically cannot hold a StateSubsystem.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Topological sort | Custom graph algorithm | Kahn's algorithm (existing pattern in SystemRegistrar + RenderGraph) | Proven twice in the codebase; adjacency list + in-degree + queue is the standard efficient implementation |
| Capability set operations | Custom bitfield or set class | `TagContainer` / `CapabilitySet` (already implemented) | HasAll, HasAny, AddTags, IsEmpty all exist with correct semantics |
| Type-erased registry | Custom RTTI or string-keyed map | `std::type_index` + `std::unordered_map` (already in SubsystemCollection) | Standard, hashable, no string overhead, matches existing pattern |
| Error propagation | `bool` + out-parameter or exceptions | `Result<void>` / `std::expected<void, Error>` (already in core/Result.h) | Project convention from copilot-instructions.md; monadic `.and_then()` for chaining |
| ECS singleton storage | Static global variable | `flecs::world::set<T>()` / `flecs::world::get<T>()` | Thread-safe via staging, no global state, scoped to world lifetime |

## Game Engine Comparison

How other engines solve the same problems, and what Wayfinder takes from each:

### Unreal Engine 5 - Subsystem Scoping

UE5's `FSubsystemCollectionBase` owns a `TMap<UClass*, USubsystem*>` per scope (Engine, GameInstance, World, LocalPlayer). Each scope has its own collection. Dependency resolution is explicit: subsystems call `Collection.InitializeDependency<T>()` during their own `Initialize()`, which triggers synchronous creation of the dependency.

**What Wayfinder takes:** Scoped collections (app vs state). But Wayfinder uses declarative DependsOn descriptors with topological sort instead of UE5's synchronous pull-based approach. Declarative is better because: (1) the full dependency graph is visible at Finalise() time for validation, (2) cycle detection happens before any construction, not as a deadlock at runtime.

**What Wayfinder avoids:** UE5's `ShouldCreateSubsystem(UObject* Outer)` takes the outer object as parameter for conditional creation. Wayfinder uses capabilities instead - more composable, no coupling to the owning object's type.

### Bevy - Plugin/Resource Model

Bevy plugins are simple `Build(&mut App)` functions. No automatic dependency ordering between plugins - `PluginGroupBuilder` has manual `add_before`/`add_after`. Resources (equivalent to subsystems) are type-erased with `TypeId`. Systems use run conditions for conditional execution.

**What Wayfinder takes:** The `Build(AppBuilder&)` plugin entry point (already decided in Phase 1). The principle that capabilities drive activation, not explicit enable/disable calls.

**What Wayfinder avoids:** Bevy's lack of automatic dependency ordering. Wayfinder's DependsOn descriptors + topological sort is a better fit for a C++ engine where compile-time and startup-time validation matter more.

### O3DE - Service Tag Model

O3DE uses CRC32 service tags for dependencies. Components declare `GetProvidedServices()`, `GetRequiredServices()`, `GetDependentServices()`, `GetIncompatibleServices()`. The framework validates all required services are satisfied before activation.

**What Wayfinder takes:** The concept of explicit provided/required service declarations. Wayfinder's capabilities (provided by plugins/states) and DependsOn (required by subsystems) serve the same role. The incompatible services concept is interesting but deferred (D-20: no optional/incompatible subsystem flags yet).

### Spartan Engine - Static Sequential Init

Spartan uses direct sequential function calls: `Window::Initialize(); Renderer::Initialize();`. Dependencies are implicit in code order. No registry, no graph, no validation.

**What Wayfinder avoids:** The entire pattern. It works for small engines but doesn't scale. Moving a line of init code silently breaks dependency assumptions. No error recovery.

### Wicked Engine - Async Job Init

Wicked dispatches independent subsystem inits to a job system. Atomic flags track completion. Dependencies are implicit (init order within a job chain).

**What Wayfinder takes:** The insight that independent subsystems can initialise in parallel. Wayfinder's topological sort naturally identifies independent nodes (same tier in the sort). This is a future optimisation opportunity, not Phase 2 scope.

## Common Pitfalls

### Pitfall 1: Dependency on Unregistered Type

**What goes wrong:** A subsystem declares `DependsOn = { typeid(Window) }` but no Window subsystem is registered. Kahn's algorithm silently ignores the missing dependency.
**Why it happens:** Copy-paste from one plugin to another without registering all dependencies.
**How to avoid:** Finalise() must explicitly validate that every type_index in every DependsOn vector corresponds to a registered subsystem. Return a clear error: "SubsystemA declares dependency on Window, but Window is not registered."
**Warning signs:** A subsystem's Initialise sees nullptr from TryGet for a declared dependency.

### Pitfall 2: Capability Set Empty After State Transition

**What goes wrong:** During a flat state transition, the old state's capabilities are removed before the new state's are applied. For a brief moment, the effective set is empty, causing all capability-gated subsystems to deactivate and immediately reactivate.
**Why it happens:** Naive implementation: remove old caps -> apply new caps, with activation checks between.
**How to avoid:** D-16 mandates batched changes. Compute the new effective set fully, then diff against the current set, then swap atomically. No intermediate empty state.
**Warning signs:** Subsystem Shutdown/Initialise calls during a transition to a state with overlapping capabilities.

### Pitfall 3: Cycle Detection Reports Wrong Path

**What goes wrong:** Kahn's algorithm detects a cycle (sorted.size() < total) but the error message says "A, B, C are in a cycle" without showing the actual cycle path.
**Why it happens:** After Kahn's, all nodes with non-zero in-degree are in OR downstream of the cycle. Not all of them form the cycle.
**How to avoid:** After Kahn's reports a cycle, run a secondary traversal on the residual graph (the nodes with non-zero in-degree). Start from any such node, follow edges, track the path. When a node is revisited, extract the cycle between the two visits.
**Warning signs:** Error messages that list too many nodes (downstream dependents, not just the cycle).

### Pitfall 4: Abstract-Type Lookup During Shutdown

**What goes wrong:** During shutdown, a subsystem tries to access another subsystem via its abstract type. But the instance is already destroyed (shutdown runs in reverse order).
**Why it happens:** Shutdown order is reverse topological. If A depends on B (B inits first), B shuts down after A. But if A accesses B via abstract type during its own Shutdown(), and B's shutdown has already removed it from the lookup...
**How to avoid:** Shutdown removes instances from the ownership container (vector of unique_ptr) but not from the lookup map until all shutdowns complete. Or: shutdown in reverse order means dependents shut down first, so the pattern naturally works if shutdown doesn't access dependents.
**Warning signs:** Null pointer access during shutdown via abstract type lookup.

### Pitfall 5: Descriptor Mutation After Finalise

**What goes wrong:** A subsystem is registered after Finalise() has been called, bypassing validation.
**Why it happens:** Registration and finalisation happen at different times. Nothing prevents calling Register after Finalise.
**How to avoid:** Finalise() sets a `m_finalised` flag. Register asserts `not m_finalised`. Follow the StateMachine pattern which already does this.
**Warning signs:** Subsystems that aren't in the dependency graph but somehow exist.

### Pitfall 6: GameSubsystems Callsite Migration

**What goes wrong:** Existing code uses `GameSubsystems::Get<T>()` which is being removed. Callsites in flecs system lambdas need to change to `world.get<EngineContextRef>()->Context->GetStateSubsystem<T>()`.
**Why it happens:** D-05 removes the static accessor. All existing callsites must migrate.
**How to avoid:** grep for all `GameSubsystems::` callsites. Each must be migrated to the ECS singleton pattern. This is a mechanical but widespread change.
**Warning signs:** Linker errors (GameSubsystems class removed) or runtime asserts (EngineContextRef not set).

## Code Examples

### SubsystemDescriptor Aggregate

```cpp
struct SubsystemDescriptor
{
    CapabilitySet RequiredCapabilities;
    std::vector<std::type_index> DependsOn;
};
```

### Deps<> Variadic Helper

A syntactic convenience for the DependsOn field:

```cpp
template<typename... TDeps>
auto Deps() -> std::vector<std::type_index>
{
    return { std::type_index(typeid(TDeps))... };
}

// Usage in descriptor:
registry.Register<PhysicsSubsystem>({
    .RequiredCapabilities = { Capability::Simulation },
    .DependsOn = Deps<AssetService, TimeSubsystem>(),
});
```

This eliminates repeated `typeid()` calls. The downside is it returns a vector (heap allocation) for each descriptor, but this runs once at startup. The agent may choose whether to include this.

### Cycle Path Extraction

After Kahn's detects a cycle (sorted.size() < total), extract the full path:

```cpp
auto ExtractCyclePath(
    const std::vector<std::vector<size_t>>& adjacency,
    const std::vector<size_t>& inDegree,
    const std::vector<std::string>& names) -> std::string
{
    // Find first node still in cycle
    size_t start = 0;
    for (size_t i = 0; i < inDegree.size(); ++i)
    {
        if (inDegree[i] > 0) { start = i; break; }
    }

    // DFS following only in-cycle nodes until revisit
    std::vector<size_t> path;
    std::vector<bool> visited(inDegree.size(), false);
    size_t current = start;

    while (not visited[current])
    {
        visited[current] = true;
        path.push_back(current);
        for (size_t next : adjacency[current])
        {
            if (inDegree[next] > 0)
            {
                current = next;
                break;
            }
        }
    }

    // current is the node where the cycle closes
    // Extract cycle from path
    std::string result;
    bool inCycle = false;
    for (size_t idx : path)
    {
        if (idx == current) inCycle = true;
        if (inCycle)
        {
            if (not result.empty()) result += " -> ";
            result += names[idx];
        }
    }
    result += " -> " + names[current]; // close the loop
    return result;
}
```

### Result<void> Fail-Fast Init Chain

```cpp
auto SubsystemRegistry<TBase>::Initialise(EngineContext& context) -> Result<void>
{
    for (size_t i = 0; i < m_initOrder.size(); ++i)
    {
        auto& entry = m_entries[m_initOrder[i]];

        // Capability gating
        if (not entry.Descriptor.RequiredCapabilities.IsEmpty()
            and not effectiveCaps.HasAll(entry.Descriptor.RequiredCapabilities))
        {
            continue; // Skip, not create
        }

        entry.Instance = entry.Factory();
        auto result = entry.Instance->Initialise(context);
        if (not result)
        {
            // Reverse-shutdown already-initialised subsystems
            for (size_t j = i; j > 0; --j)
            {
                if (m_entries[m_initOrder[j - 1]].Instance)
                {
                    m_entries[m_initOrder[j - 1]].Instance->Shutdown();
                }
            }
            return std::unexpected(result.error());
        }

        m_lookup[entry.ConcreteType] = entry.Instance.get();
        if (entry.AbstractType.has_value())
        {
            m_lookup[entry.AbstractType.value()] = entry.Instance.get();
        }
    }
    return {};
}
```

### EngineContextRef Flecs Singleton

```cpp
namespace Wayfinder
{
    /// Flecs singleton component providing access to the engine context.
    /// Set on state entry, removed on state exit.
    struct EngineContextRef
    {
        EngineContext* Context = nullptr;
    };
}

// In GameplayState::OnEnter:
simulation.GetWorld().set<EngineContextRef>({.Context = &context});

// In flecs system:
world.system<Transform, RigidBody>()
    .each([](flecs::entity e, Transform& t, RigidBody& rb)
    {
        auto* ref = e.world().get<EngineContextRef>();
        if (not ref or not ref->Context) return;
        auto* physics = ref->Context->TryGetStateSubsystem<PhysicsSubsystem>();
        if (physics) physics->SyncTransform(t, rb);
    });

// In GameplayState::OnExit:
simulation.GetWorld().remove<EngineContextRef>();
```

### Subsystem Base Class Signature Change

Current `Subsystem::Initialise()` takes no arguments. The v2 signature takes `EngineContext&` and returns `Result<void>`:

```cpp
class Subsystem
{
public:
    virtual ~Subsystem() = default;

    [[nodiscard]] virtual auto Initialise(EngineContext& context) -> Result<void>
    {
        return {};
    }

    virtual void Shutdown() {}

    virtual auto ShouldCreate() const -> bool
    {
        return true;
    }
};
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| SubsystemCollection (registration-order init) | SubsystemRegistry (topological-order init) | Phase 2 | Init order determined by dependency graph, not registration order. Shutdown reversed automatically. |
| GameSubsystems static accessor | ECS singleton EngineContextRef | Phase 2 | No global mutable state. Thread-safe via flecs staging. Lifetime bounded to state. |
| EngineContext 27-line struct | EngineContext non-owning facade class | Phase 2 | Central service access for all states, overlays, subsystems. Assert stubs for Phase 4 features. |
| Capability tags (declared only) | Capability-gated activation | Phase 2 | Subsystems activate/deactivate based on effective capability set |
| Manual error checking (if/return) | Result<void> with fail-fast | Phase 2 | First init failure aborts sequence and cleans up |

## Open Questions

1. **Subsystem base class virtual Initialise signature change**
   - What we know: Current base classes (Subsystem, AppSubsystem, StateSubsystem) have `virtual void Initialise()`. The v2 signature is `Initialise(EngineContext&) -> Result<void>`.
   - What's unclear: Whether to modify the existing base class signatures (breaking existing overrides) or introduce the new signature alongside the old one temporarily.
   - Recommendation: Modify in-place. This is a greenfield project with no external consumers. The old signature overrides will produce compiler errors, which guides migration. Mark the old callsites with the new signature during this phase.

2. **Subsystem::ShouldCreate() retention**
   - What we know: SubsystemCollection checks `ShouldCreate()` after constructing an instance. SubsystemRegistry uses capabilities for pre-construction gating.
   - What's unclear: Whether to keep `ShouldCreate()` as an additional post-construction check or remove it since capabilities handle the use case.
   - Recommendation: Remove it. Capabilities (D-20) are the optionality mechanism. If a subsystem's RequiredCapabilities aren't met, it's never created. `ShouldCreate()` was the pre-capability workaround. Keep the method in the base class but don't call it from SubsystemRegistry.

3. **CMakeLists.txt impact**
   - What we know: EngineContext.cpp is new. No new headers (EngineContext.h exists and is modified in-place).
   - What's unclear: Whether Subsystem.h needs to be split (SubsystemRegistry in its own file) given it will grow significantly.
   - Recommendation: SubsystemRegistry is a template and lives entirely in a header. Create a new `SubsystemRegistry.h` alongside `Subsystem.h`. Keep the base classes in `Subsystem.h`. Move `GameSubsystems` to a separate file or remove it entirely (D-05) and repoint callsites.

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | doctest (in-project, thirdparty/doctest) |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `ctest --preset test -R SubsystemTests` |
| Full suite command | `ctest --preset test` |

### Phase Requirements -> Test Map

| Req ID | Behaviour | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| SUB-02 | Topological init order, cycle detection at Finalise | unit | `ctest --preset test -R SubsystemTests` | Exists (`tests/app/SubsystemTests.cpp`) - needs new test cases |
| SUB-03 | Capability-gated activation skips subsystems | unit | `ctest --preset test -R SubsystemTests` | Needs new test cases |
| SUB-04 | Abstract-type resolution, duplicate detection | unit | `ctest --preset test -R SubsystemTests` | Needs new test cases |
| SUB-05 | Initialise returns Result<void>, fail-fast cleanup | unit | `ctest --preset test -R SubsystemTests` | Needs new test cases |
| SUB-06 | EngineContext typed access to registries | unit | `ctest --preset test -R SubsystemTests` | Needs new test cases |
| SUB-07 | GameSubsystems removal, EngineContextRef singleton | unit | `ctest --preset test -R SubsystemTests` | Needs new test cases |
| CAP-02 | Two-source capability union | unit | `ctest --preset test -R SubsystemTests` | Needs new test cases (or extend existing CapabilityTests if present) |
| CAP-03 | Uniform activation predicate | unit | `ctest --preset test -R SubsystemTests` | Needs new test cases |
| CAP-04 | Empty RequiredCapabilities = always active | unit | `ctest --preset test -R SubsystemTests` | Needs new test cases |
| CAP-05 | Batched capability swap | unit | `ctest --preset test -R SubsystemTests` | Needs new test cases |

### Sampling Rate
- **Per task commit:** `cmake --build --preset debug --target wayfinder_core_tests && ctest --preset test -R SubsystemTests`
- **Per wave merge:** `cmake --build --preset debug && ctest --preset test`
- **Phase gate:** Full suite green before /gsd-verify-work

### Wave 0 Gaps
- Existing `SubsystemTests.cpp` covers old SubsystemCollection API. New test cases needed for: dependency ordering, cycle detection, capability gating, abstract-type resolution, Result propagation, EngineContext v2, EngineContextRef singleton.
- No new test files needed - extend existing `tests/app/SubsystemTests.cpp`.

## Sources

### Primary (HIGH confidence)

- **Existing codebase** - SubsystemCollection<TBase> (engine/wayfinder/src/app/Subsystem.h), SystemRegistrar Kahn's (engine/wayfinder/src/plugins/registrars/SystemRegistrar.cpp), RenderGraph Kahn's (engine/wayfinder/src/rendering/graph/RenderGraph.cpp), TagContainer (engine/wayfinder/src/gameplay/Tag.h), Result<T> (engine/wayfinder/src/core/Result.h)
- **Architecture spec** - docs/plans/application_architecture_v2.md (SubsystemRegistry design, EngineContext v2, capability system)
- **Migration spec** - docs/plans/application_migration_v2.md (rename/keep/remove/add tables)
- **Game framework spec** - docs/plans/game_framework.md (ServiceProvider, Simulation, GameplayState patterns)
- **C++23 standard** - std::expected monadic operations, concepts/requires, std::type_index, designated initialisers (Clang 22.1 full support)

### Secondary (MEDIUM confidence)

- **Unreal Engine 5** - FSubsystemCollectionBase pattern, scoped collections, InitializeDependency pull model
- **Bevy Engine** - Plugin::Build model, TypeId-based registry, PluginGroupBuilder manual ordering
- **O3DE** - ComponentDescriptor GetRequiredServices/GetProvidedServices pattern, CRC service tags
- **flecs** - world.set<T>/world.get<T> singleton pattern, staging model for thread safety

### Tertiary (LOW confidence)

- **Spartan Engine / Wicked Engine** - Sequential/job-based init patterns (informational only, not adopted)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All components already exist in-project or in C++23 stdlib
- Architecture: HIGH - Detailed architecture spec exists; decisions locked in CONTEXT.md; existing code provides foundation
- Pitfalls: HIGH - Based on actual codebase patterns and engine industry experience
- C++23 features: HIGH - Clang 22.1 has full C++23 support; std::expected/concepts/type_index all verified present in project

**Research date:** 2026-04-04
**Valid until:** Indefinite (architecture-focused, not library-version-dependent)
