# Technology Stack: C++23 Patterns for Application Architecture Migration

**Project:** Wayfinder v2 Application Architecture Migration
**Researched:** 2026-04-03
**Overall confidence:** HIGH (language-level patterns, no external library risk)

---

## Executive Summary

This document covers C++23 patterns, idioms, and techniques for implementing the six architectural pillars of the v2 migration: application state machines, capability/tag systems, type-erased service providers, subsystem registries with dependency ordering, plugin composition with typed registrars, and generic state machines.

The engine already has strong C++23 foundations (`Result<T>` via `std::expected`, `InternedString`, `GameplayTag`, generational `Handle<T>`, concepts on `OpaqueHandleTag`). The migration extends these patterns into the application shell and game framework layers. No new libraries are needed -- this is pure language-level technique.

**Key insight:** The v2 architecture is fundamentally a *type-erased service composition* problem. C++23's concepts, deducing `this`, and `std::expected` form a coherent toolkit for expressing it cleanly.

---

## Recommended Patterns

### 1. Application State Machine (Push/Pop Stack)

**Problem:** Hybrid flat transitions + push/pop modal stack with suspension negotiation and deferred processing.

**Confidence:** HIGH -- standard patterns, well-understood.

#### Pattern: Variant-Free Polymorphic State Stack

The `ApplicationStateMachine` manages `IApplicationState*` through a combination of an active state pointer and a stack for push/pop. This is one of the few places in the engine where virtual dispatch is the correct tool -- states are few (< 10), transitions are rare (frame boundaries), and the lifecycle interface is genuinely polymorphic.

```cpp
class ApplicationStateMachine
{
public:
    void ProcessPending(EngineContext& ctx);
    void DispatchEvent(EngineContext& ctx, Event& event);
    void Update(EngineContext& ctx, float dt);
    void Render(EngineContext& ctx);

private:
    struct PendingTransition
    {
        std::type_index TargetType;
        bool IsPush = false;
    };

    IApplicationState* m_active = nullptr;
    std::vector<IApplicationState*> m_stack;           // push/pop stack (not including active)
    std::optional<PendingTransition> m_pending;
    std::unordered_map<std::type_index, std::unique_ptr<IApplicationState>> m_states;
};
```

#### C++23-Specific Techniques

**`std::derived_from` constraint on transition requests:**

```cpp
template<std::derived_from<IApplicationState> T>
void EngineContext::RequestTransition()
{
    m_pendingTransition = PendingTransition{
        .TargetType = std::type_index(typeid(T)),
        .IsPush = false,
    };
}

template<std::derived_from<IApplicationState> T>
void EngineContext::RequestPush()
{
    m_pendingTransition = PendingTransition{
        .TargetType = std::type_index(typeid(T)),
        .IsPush = true,
    };
}
```

This catches misuse at compile time -- passing a non-state type to `RequestTransition<int>()` is a hard error.

**`Result<void>` for lifecycle fallibility:**

```cpp
auto IApplicationState::OnEnter(EngineContext& ctx) -> Result<void>;
```

`OnEnter` returning `Result<void>` lets the state machine handle failure gracefully (abort transition, roll back to previous state, or transition to an error state). This is better than exceptions because:
1. State transitions are expected to fail sometimes (missing assets, hardware constraints)
2. The state machine must decide the recovery policy, not the caller
3. Hot-path code (Update/Render) remains `void` -- only entry/exit can fail

**Designated initialisers for suspension negotiation:**

```cpp
auto PauseState::GetBackgroundPreferences() const -> BackgroundPreferences
{
    return { .WantsBackgroundRender = true, .WantsBackgroundUpdate = false };
}
```

Clean, self-documenting, and impossible to confuse parameter order.

#### Anti-Patterns

| Don't | Why | Do Instead |
|-------|-----|------------|
| `std::variant<SplashState, GameplayState, ...>` | Closed set, forces recompilation on new states, breaks plugin extensibility | Polymorphic `IApplicationState*` with `type_index` lookup |
| Coroutine-based state machine | Tempting for sequential flow, but state lifetimes don't map to coroutine frames -- states suspend/resume, aren't consumed | Explicit lifecycle methods |
| `dynamic_cast` for state identification | Fragile, slow, unclear intent | `type_index` keying + `GetName()` for debugging |
| Immediate transitions | Race conditions with event processing, subsystem teardown during frame | Deferred processing at frame boundaries |

---

### 2. Capability/Feature-Flag System Using Tag Hierarchies

**Problem:** Activation gating for subsystems, overlays, and render features using composable, hierarchical identifiers.

**Confidence:** HIGH -- builds directly on existing `GameplayTag` + `InternedString` infrastructure.

#### Pattern: GameplayTag Set Algebra

Capabilities are `GameplayTag` values. The effective capability set is the union of app-level and state-level tags. Activation checks are set-containment queries: "does the effective set contain all required tags?"

```cpp
class CapabilitySet
{
public:
    void Add(GameplayTag tag) { m_tags.insert(tag); }
    void Remove(GameplayTag tag) { m_tags.erase(tag); }
    void Clear() { m_tags.clear(); }

    /// Check if all required capabilities are satisfied.
    [[nodiscard]] bool Satisfies(std::span<const GameplayTag> required) const
    {
        return std::ranges::all_of(required, [this](const GameplayTag& tag) {
            return m_tags.contains(tag);
        });
    }

    /// Merge another set (union).
    void Merge(const CapabilitySet& other)
    {
        m_tags.insert(other.m_tags.begin(), other.m_tags.end());
    }

private:
    std::flat_set<GameplayTag> m_tags;  // cache-friendly, small N
};
```

#### C++23-Specific Techniques

**`std::flat_set` for small, cache-friendly tag sets:**

Capability sets are small (typically 2-6 tags). `std::flat_set` stores elements in a contiguous `std::vector`, giving cache-friendly iteration and O(log N) lookup -- far better than `std::set` for this cardinality. `GameplayTag` has `operator<=>` already defined.

**`std::span<const GameplayTag>` for non-owning requirement lists:**

Registration descriptors hold `std::vector<GameplayTag>` for the owned requirement list; all query interfaces accept `std::span<const GameplayTag>` so callers can pass temporaries, arrays, or vectors without copying.

**`std::ranges::all_of` for set-containment:**

Clean, declarative, and generates the same code as a hand-rolled loop. Reads as specification: "the set satisfies requirements when all required tags are present."

**Hierarchical matching with `IsChildOf`:**

The existing `GameplayTag::IsChildOf` enables hierarchical gating. A subsystem requiring `Capability.Rendering` is satisfied by `Capability.Rendering.GPU` or `Capability.Rendering.Software`:

```cpp
[[nodiscard]] bool SatisfiesHierarchical(std::span<const GameplayTag> required) const
{
    return std::ranges::all_of(required, [this](const GameplayTag& req) {
        return std::ranges::any_of(m_tags, [&req](const GameplayTag& tag) {
            return tag == req or tag.IsChildOf(req);
        });
    });
}
```

#### Anti-Patterns

| Don't | Why | Do Instead |
|-------|-----|------------|
| `enum class Capability { Simulation, Rendering, ... }` | Closed set, can't be extended by plugins, no hierarchy, grows into a god-enum | `GameplayTag` -- open set, hierarchical, plugin-definable |
| Bitfield flags (`uint64_t`) | Max 64 capabilities, no hierarchy, no diagnostic names | `flat_set<GameplayTag>` |
| String comparison for tag matching | O(N) per comparison | `InternedString` backs `GameplayTag` -- O(1) equality |
| Global mutable capability state | Thread-unsafe, unclear ownership, spooky action at distance | `CapabilitySet` owned by `EngineContext`, updated at frame boundaries |

---

### 3. Type-Erased Service Locator / Dependency Injection (ServiceProvider)

**Problem:** `Simulation` must access engine services (AssetService, TagRegistry) without coupling to `EngineContext` or any specific container. Headless tests need a different provider.

**Confidence:** HIGH -- this is the canonical C++20/23 concept-based type erasure pattern.

#### Pattern: Concept-Constrained ServiceProvider

The `ServiceProvider` concept defines what `Simulation` needs. Any type satisfying the concept can be used -- no inheritance, no virtual dispatch, no `void*`:

```cpp
template<typename T>
concept ServiceProvider = requires(T provider) {
    { provider.template TryGet<struct AnyService>() } -> std::same_as<struct AnyService*>;
};
```

However, this concept as written checks a single concrete type. The real constraint is structural: "has a `TryGet<U>()` that returns `U*` for any `U`." In C++23, this is best expressed as a structural concept that checks the interface shape:

```cpp
/// Concept: a type that provides TryGet<T>() -> T* for service lookup.
/// We check a canary type to validate the template interface exists.
/// The actual types queried are validated at call sites.
namespace Detail
{
    struct ServiceProviderProbe {};
}

template<typename T>
concept ServiceProvider = requires(T& provider) {
    { provider.template TryGet<Detail::ServiceProviderProbe>() }
        -> std::convertible_to<Detail::ServiceProviderProbe*>;
};
```

Then `Simulation` is:

```cpp
class Simulation
{
public:
    auto Initialise(ServiceProvider auto& services) -> Result<void>
    {
        m_assets = services.template TryGet<AssetService>();
        m_tags = services.template TryGet<GameplayTagRegistry>();
        return {};
    }
};
```

#### Two Concrete Providers

**EngineContextServiceProvider** (thin adapter for live application):

```cpp
struct EngineContextServiceProvider
{
    EngineContext& Ctx;

    template<typename T>
    auto TryGet() -> T* { return Ctx.TryGetAppSubsystem<T>(); }

    template<typename T>
    auto Get() -> T&
    {
        auto* ptr = TryGet<T>();
        assert(ptr and "Required service not available");
        return *ptr;
    }
};
```

**StandaloneServiceProvider** (type-erased registry for tests/tools):

```cpp
class StandaloneServiceProvider
{
public:
    template<typename T>
    void Register(T& service)
    {
        m_services[std::type_index(typeid(T))] = std::addressof(service);
    }

    template<typename T>
    auto TryGet() -> T*
    {
        if (auto it = m_services.find(std::type_index(typeid(T))); it != m_services.end())
            return static_cast<T*>(it->second);
        return nullptr;
    }

    template<typename T>
    auto Get() -> T&
    {
        auto* ptr = TryGet<T>();
        assert(ptr and "Service not registered in StandaloneServiceProvider");
        return *ptr;
    }

private:
    std::unordered_map<std::type_index, void*> m_services;
};
```

#### Why Concepts Over Virtual Interface

| Aspect | Concept (`ServiceProvider auto&`) | Virtual (`IServiceProvider&`) |
|--------|----------------------------------|-------------------------------|
| Dispatch | Static (monomorphised at compile time) | Dynamic (vtable indirection) |
| Interface evolution | Add new service types without modifying the concept | Add new virtual methods = ABI break |
| Memory layout | No pointer indirection, inline in caller | Heap allocation, vtable pointer overhead |
| Error messages | Clear concept-violation diagnostics | Opaque "no matching override" |
| Test ergonomics | Aggregate struct satisfies concept | Must inherit interface, implement all methods |

The concept approach is strictly better here because:
1. There are exactly two providers (live and standalone) -- not an open set
2. Service lookup is on the init path, not the hot path -- but concept dispatch is still cleaner
3. New service types are added by adding `TryGet<NewType>()` calls at usage sites, not by modifying a central interface

#### Anti-Patterns

| Don't | Why | Do Instead |
|-------|-----|------------|
| `std::any` for service storage | Type-unsafe at retrieval, runtime `bad_any_cast`, no compile-time validation | `type_index` -> `void*` with `static_cast` (type safety from the template wrapper) |
| Singleton/global service locator | Untestable, hidden dependencies, unclear lifetime | Concept-constrained parameter passed explicitly |
| Constructor injection for all services | `Simulation` doesn't know at construction time which services will exist | Init-time lookup via `ServiceProvider` |
| `dynamic_cast` for service retrieval | Requires RTTI on a common base, slower than `type_index` lookup | `type_index` keyed map + `static_cast` |

---

### 4. Subsystem Registries with Dependency Ordering (Topological Sort)

**Problem:** Subsystems declare dependencies; init must follow dependency order; shutdown must reverse it. Both app-scoped and state-scoped registries.

**Confidence:** HIGH -- Kahn's algorithm already proven in `SystemRegistrar.cpp`.

#### Pattern: SubsystemRegistry with Type-Indexed Dependency Graph

Evolve the existing `SubsystemCollection` into `SubsystemRegistry` with explicit dependency declaration and topological ordering:

```cpp
struct SubsystemDescriptor
{
    std::type_index Type;
    std::type_index AbstractType;                  // for interface-based lookup (optional)
    std::function<std::unique_ptr<Subsystem>()> Factory;
    std::vector<std::type_index> DependsOn;
    std::vector<GameplayTag> RequiredCapabilities;
};
```

#### C++23-Specific Techniques

**`Result<void>` on `Initialise()` for ordered rollback:**

```cpp
class AppSubsystem : public Subsystem
{
public:
    virtual auto Initialise(SubsystemRegistry& registry) -> Result<void> { return {}; }
};
```

When `Initialise()` returns an error, the registry shuts down already-initialised subsystems in reverse order. This is a clean transaction model:

```cpp
auto SubsystemRegistry::InitialiseAll(const CapabilitySet& capabilities) -> Result<void>
{
    auto sorted = TopologicalSort(m_descriptors);
    if (not sorted)
        return std::unexpected(sorted.error());

    for (const auto& desc : *sorted)
    {
        if (not capabilities.Satisfies(desc.RequiredCapabilities))
            continue;

        auto instance = desc.Factory();
        auto result = instance->Initialise(*this);
        if (not result)
        {
            // Rollback: shutdown already-initialised in reverse
            ShutdownInReverse();
            return std::unexpected(result.error());
        }
        Store(desc.Type, desc.AbstractType, std::move(instance));
    }
    return {};
}
```

**`std::ranges::to<std::vector>()` for materialising sorted results:**

```cpp
auto TopologicalSort(std::span<const SubsystemDescriptor> descriptors)
    -> Result<std::vector<const SubsystemDescriptor*>>
{
    // ... Kahn's algorithm (existing pattern from SystemRegistrar) ...

    if (sorted.size() != descriptors.size())
    {
        auto cyclic = descriptors
            | std::views::filter([&](const auto& d) { return inDegree[&d] > 0; })
            | std::views::transform([](const auto& d) { return d.Type.name(); })
            | std::ranges::to<std::vector>();

        return MakeError(std::format("Cycle in subsystem dependencies: {}",
            fmt::join(cyclic, ", ")));
    }

    return sorted;
}
```

**Dual registration with abstract type for interface-based lookup:**

```cpp
template<std::derived_from<Subsystem> TConcrete, typename TAbstract = TConcrete>
void RegisterSubsystem(SubsystemRegistrationOptions opts)
{
    m_descriptors.push_back({
        .Type = std::type_index(typeid(TConcrete)),
        .AbstractType = std::type_index(typeid(TAbstract)),
        .Factory = []() -> std::unique_ptr<Subsystem> {
            return std::make_unique<TConcrete>();
        },
        .DependsOn = std::move(opts.DependsOn),
        .RequiredCapabilities = std::move(opts.RequiredCapabilities),
    });
}
```

This enables `ctx.GetAppSubsystem<Window>()` returning `SDL3Window&` without the caller knowing the concrete type -- same `type_index` trick already in `SubsystemCollection`, extended with the abstract indirection.

#### Anti-Patterns

| Don't | Why | Do Instead |
|-------|-----|------------|
| Manual init ordering (register in the right order) | Fragile, breaks when new subsystems are added | Declared dependencies + topological sort |
| Single global registry | App-scoped and state-scoped have different lifetimes | Two `SubsystemRegistry` instances in `EngineContext` |
| Circular dependency tolerance | Indicates architectural problem | Detect cycles at startup, log involved types, abort with `Result<void>` error |
| Lazy initialisation (init on first `Get<T>()`) | Order-dependent, hard to reason about, unclear failure points | Eager ordered init with explicit capability gating |

---

### 5. Plugin Composition with Typed Registrar Stores (AppBuilder)

**Problem:** `AppBuilder` must accept diverse registration types (states, subsystems, overlays, ECS systems, tags, render features, custom domain registrars) through a single builder API. Registrars are type-keyed: each domain owns its validation logic.

**Confidence:** HIGH -- type-erased heterogeneous containers are well-established in C++.

#### Pattern: Type-Indexed Registrar Container

`AppBuilder` holds a `std::unordered_map<std::type_index, std::unique_ptr<IRegistrarBase>>` where each registrar is a domain-specific type with its own validation and finalisation logic:

```cpp
class IRegistrarBase
{
public:
    virtual ~IRegistrarBase() = default;
    virtual auto Validate() -> Result<void> = 0;
};

class AppBuilder
{
public:
    /// Get or create a typed registrar.
    template<std::derived_from<IRegistrarBase> T>
    auto Registrar() -> T&
    {
        auto key = std::type_index(typeid(T));
        if (auto it = m_registrars.find(key); it != m_registrars.end())
            return static_cast<T&>(*it->second);

        auto [it, _] = m_registrars.emplace(key, std::make_unique<T>());
        return static_cast<T&>(*it->second);
    }

    /// Convenience: register a state (delegates to StateRegistrar).
    template<std::derived_from<IApplicationState> T>
    void AddState(StateRegistrationOptions opts)
    {
        Registrar<StateRegistrar>().Register<T>(std::move(opts));
    }

    /// Convenience: register an app or state subsystem.
    /// Routing is determined by the base class at compile time.
    template<std::derived_from<Subsystem> T, typename TAbstract = T>
    void RegisterSubsystem(SubsystemRegistrationOptions opts)
    {
        if constexpr (std::derived_from<T, AppSubsystem>)
            Registrar<AppSubsystemRegistrar>().Register<T, TAbstract>(std::move(opts));
        else if constexpr (std::derived_from<T, StateSubsystem>)
            Registrar<StateSubsystemRegistrar>().Register<T, TAbstract>(std::move(opts));
        else
            static_assert(ALWAYS_FALSE<T>, "Subsystem must derive from AppSubsystem or StateSubsystem");
    }

    /// Finalise: validate all registrars and produce read-only AppDescriptor.
    [[nodiscard]] auto Finalise() -> Result<AppDescriptor>;

private:
    std::unordered_map<std::type_index, std::unique_ptr<IRegistrarBase>> m_registrars;
};
```

#### C++23-Specific Techniques

**`if constexpr` + `std::derived_from` for compile-time routing:**

The `RegisterSubsystem` method above uses `if constexpr` with `std::derived_from` to route registrations to the correct registrar at compile time. No runtime branching, no SFINAE, clear error message via `ALWAYS_FALSE<T>` in the terminal branch.

**`std::derived_from` constraints on all registration methods:**

Every `Add*` / `Register*` method constrains its type parameter:

```cpp
template<std::derived_from<IPlugin> T, typename... TArgs>
void Application::AddPlugin(TArgs&&... args);

template<std::derived_from<IApplicationState> T>
void AppBuilder::AddState(StateRegistrationOptions opts);

template<std::derived_from<IOverlay> T>
void AppBuilder::RegisterOverlay(OverlayRegistrationOptions opts);
```

This turns "forgot to inherit from IPlugin" from a linker error into a clear concept-violation diagnostic at the call site.

**Per-state builder via `ForState<T>()`:**

```cpp
template<std::derived_from<IApplicationState> T>
auto AppBuilder::ForState() -> StateBuilder<T>&
{
    auto key = std::type_index(typeid(T));
    if (auto it = m_stateBuilders.find(key); it != m_stateBuilders.end())
        return static_cast<StateBuilder<T>&>(*it->second);

    auto [it, _] = m_stateBuilders.emplace(key, std::make_unique<StateBuilder<T>>());
    return static_cast<StateBuilder<T>&>(*it->second);
}
```

**Designated initialisers throughout registration options:**

```cpp
builder.RegisterSubsystem<PhysicsSubsystem>({
    .RequiredCapabilities = { Capability::Simulation },
    .DependsOn = { typeid(AssetService) },
    .Config = PhysicsConfig{ .Gravity = {0.f, -9.81f, 0.f} },
});
```

These are aggregates -- no constructors needed, clear field names, impossible to mix up parameter order.

**Custom registrar extensibility (plugin-defined registrars):**

```cpp
class ReplicationRegistrar : public IRegistrarBase
{
public:
    void Register(ReplicationDescriptor desc) { m_descriptors.push_back(std::move(desc)); }
    auto Validate() -> Result<void> override { /* validate policies */ }
    auto GetDescriptors() const -> std::span<const ReplicationDescriptor> { return m_descriptors; }
private:
    std::vector<ReplicationDescriptor> m_descriptors;
};

// In plugin:
builder.Registrar<ReplicationRegistrar>().Register({
    .ComponentType = typeid(Transform),
    .Policy = ReplicationPolicy::Interpolated,
});
```

No `AppBuilder` modification needed. The type-indexed container creates the registrar on first access.

#### Anti-Patterns

| Don't | Why | Do Instead |
|-------|-----|------------|
| Mega-builder with all registration methods inline | Grows unbounded, every new domain modifies AppBuilder | Type-indexed registrar container; convenience methods are thin wrappers |
| `std::any` for config storage | No compile-time safety, `bad_any_cast` at runtime | Typed registrar stores with `static_cast` behind type-guarded access |
| Registration after `Finalise()` | Mutation after validation breaks invariants | `AppBuilder` consumed by `Finalise()` (move-only), returns immutable `AppDescriptor` |
| Virtual `Build()` on concrete registrars | Over-abstraction; registrars are domain-specific, not interchangeable | Only `IRegistrarBase` is virtual (for `Validate()`); concrete registrars have typed APIs |

---

### 6. Generic State Machine (StateMachine\<TStateId\>)

**Problem:** A reusable flat state machine template for sub-state management within `IApplicationState` implementations. Uses `InternedString` as state ID for sub-states, but should be generic.

**Confidence:** HIGH -- straightforward template design.

#### Pattern: Value-Typed Flat State Machine

```cpp
template<typename TStateId>
struct StateCallbacks
{
    std::function<void(EngineContext&)> OnEnter;
    std::function<void(EngineContext&)> OnExit;
};

template<typename TStateId>
class StateMachine
{
public:
    using TransitionCallback = std::function<void(TStateId from, TStateId to)>;

    void RegisterState(TStateId id, StateCallbacks<TStateId> callbacks)
    {
        m_states.emplace(id, std::move(callbacks));
    }

    void TransitionTo(TStateId target)
    {
        m_pending = target;
    }

    void OnTransition(TransitionCallback cb)
    {
        m_observers.push_back(std::move(cb));
    }

    void ProcessPending(EngineContext& ctx)
    {
        if (not m_pending)
            return;

        auto target = *m_pending;
        m_pending.reset();

        if (m_current == target)
            return;

        // Exit current
        if (auto it = m_states.find(m_current); it != m_states.end())
        {
            if (it->second.OnExit)
                it->second.OnExit(ctx);
        }

        m_previous = m_current;
        m_current = target;

        // Enter new
        if (auto it = m_states.find(m_current); it != m_states.end())
        {
            if (it->second.OnEnter)
                it->second.OnEnter(ctx);
        }

        // Notify observers
        for (const auto& observer : m_observers)
            observer(m_previous, m_current);
    }

    void Start(EngineContext& ctx)
    {
        if (auto it = m_states.find(m_current); it != m_states.end())
        {
            if (it->second.OnEnter)
                it->second.OnEnter(ctx);
        }
    }

    [[nodiscard]] auto GetCurrentState() const -> TStateId { return m_current; }
    [[nodiscard]] auto GetPreviousState() const -> TStateId { return m_previous; }

private:
    TStateId m_current{};
    TStateId m_previous{};
    std::optional<TStateId> m_pending;
    std::unordered_map<TStateId, StateCallbacks<TStateId>> m_states;
    std::vector<TransitionCallback> m_observers;
};
```

#### C++23-Specific Techniques

**`std::optional<TStateId>` for deferred transitions:**

Deferred transitions are the correct model -- they avoid re-entrancy bugs where a callback triggers another transition during `ProcessPending`. `std::optional` makes the "no pending transition" state explicit without sentinels.

**Constraint on `TStateId`:**

```cpp
template<typename T>
concept StateIdentifier = std::equality_comparable<T>
    and std::default_initializable<T>
    and std::copyable<T>;

template<StateIdentifier TStateId>
class StateMachine { ... };
```

This documents the requirements on `TStateId` and provides clear diagnostics if someone tries `StateMachine<std::unique_ptr<int>>`.

**`std::flat_map` for state storage (when N is small):**

For sub-state machines (typically 3-8 states), `std::flat_map<TStateId, StateCallbacks<TStateId>>` gives cache-friendly lookup. For the rare case of many states, `std::unordered_map` is the fallback -- but sub-state machines should stay small.

**Observer pattern with `std::function` (correct here):**

`std::function` is sometimes criticised for heap allocation, but sub-state machines have 1-2 observers wired at setup time. The ergonomic benefit (lambdas, captures) far outweighs the allocation cost. This is not a hot path.

#### Anti-Patterns

| Don't | Why | Do Instead |
|-------|-----|------------|
| Coroutine state machine | States have persistent member data and suspend/resume semantics that don't map to coroutine frames | Explicit state objects with lifecycle callbacks |
| Sharing a base with `ApplicationStateMachine` | Different responsibilities: one is flat + value-typed, other is polymorphic + push/pop | Two separate types, no shared base |
| `enum class` for sub-state IDs | Closed set, can't be extended by plugins | `InternedString` -- open set, plugin-definable |
| Immediate transition in `TransitionTo()` | Re-entrancy if OnEnter triggers another transition | Always defer, process in `ProcessPending()` |
| `std::variant` for state data | Sub-state data belongs to the owning `IApplicationState`, not the state machine | State machine manages transitions only; state-specific data lives in the parent |

---

## Cross-Cutting Patterns

### Deducing `this` for CRTP-Free Mixins

Deducing `this` (C++23) eliminates CRTP for common patterns. Potential use in subsystem or overlay helpers:

```cpp
class SubsystemMixin
{
public:
    /// Get a sibling subsystem from the registry. Deducing this
    /// avoids CRTP while preserving the concrete type for error messages.
    template<typename T>
    auto GetSibling(this auto&& self, SubsystemRegistry& registry) -> T&
    {
        auto* ptr = registry.TryGet<T>();
        assert(ptr and "Missing sibling subsystem");
        return *ptr;
    }
};
```

**Confidence:** MEDIUM -- deducing `this` is C++23 but Clang support should be verified. The fallback (explicit template parameter) is trivial.

### `[[nodiscard]]` Discipline

Apply `[[nodiscard]]` to:
- All `Result<T>` returning functions (already the pattern)
- `StateMachine::GetCurrentState()` / `GetPreviousState()`
- `CapabilitySet::Satisfies()`
- `SubsystemRegistry::TryGet<T>()`
- `AppBuilder::Finalise()` (critical -- the return value IS the product)

### `noexcept` Strategy

- Move constructors/assignment on all value types: `noexcept`
- Destructors: implicitly `noexcept`
- `StateMachine::GetCurrentState()`: `noexcept` (trivial accessor)
- `Initialise()`, `OnEnter()`: NOT `noexcept` (return `Result<void>`, may construct resources)
- `ProcessPending()`: NOT `noexcept` (calls user callbacks)

### `constexpr` Where Possible

Capability tag definitions should be `constexpr` or `inline const`:

```cpp
namespace Capability
{
    inline const GameplayTag Simulation = GameplayTag::FromName("Capability.Simulation");
    inline const GameplayTag Rendering = GameplayTag::FromName("Capability.Rendering");
    inline const GameplayTag Presentation = GameplayTag::FromName("Capability.Presentation");
    inline const GameplayTag Editing = GameplayTag::FromName("Capability.Editing");
}
```

Note: `GameplayTag::FromName` interns a string at runtime (global table lookup), so these are `inline const` rather than `constexpr`. This is correct -- interning must happen at runtime.

---

## Summary: Pattern-to-Feature Matrix

| v2 Feature | Primary C++23 Pattern | Key Types |
|------------|----------------------|-----------|
| ApplicationStateMachine | `std::derived_from` constraints, `Result<void>`, designated init | `IApplicationState`, `EngineContext` |
| Capability system | `std::flat_set`, `std::ranges::all_of`, `std::span` | `CapabilitySet`, `GameplayTag` |
| ServiceProvider | Concepts as type erasure, `ServiceProvider auto&` | `ServiceProvider` concept, adapters |
| SubsystemRegistry | `Result<void>` rollback, topological sort, `type_index` dual-key | `SubsystemDescriptor`, `AppSubsystem`, `StateSubsystem` |
| AppBuilder / Plugins | Type-indexed registrar container, `if constexpr` routing, `std::derived_from` | `AppBuilder`, `IRegistrarBase`, domain registrars |
| StateMachine\<T\> | `std::optional` deferred transitions, `StateIdentifier` concept, `std::flat_map` | `StateMachine<InternedString>`, `StateCallbacks<T>` |

---

## What NOT to Use (Engine-Wide Anti-Patterns)

| Technique | Why Not | What Instead |
|-----------|---------|-------------|
| `std::any` | Type-unsafe at retrieval, runtime exceptions, no compile-time validation | `type_index` -> `void*` with template wrappers, or concepts |
| `dynamic_cast` for service lookup | Requires common base, slower than map lookup, unclear intent | `type_index` keyed maps |
| Exceptions for control flow | State transitions and init failures are expected, not exceptional | `Result<void>` via `std::expected` |
| `std::shared_ptr` for subsystem ownership | Unclear ownership, reference cycles, overhead | `std::unique_ptr` in registry, raw pointers/references for non-owning access |
| Singletons / globals | Untestable, unclear lifetime, hidden dependencies | Explicit context passing (`EngineContext&`) -- only `StateSubsystems::Get<T>()` as bounded escape hatch for flecs callbacks |
| Macro-based registration | Fragile, no type safety, hard to debug | Template-based registration with concept constraints |
| Immediate state transitions | Re-entrancy, partial state, mid-frame teardown | Always deferred to frame boundaries |

---

## Compiler/Toolchain Notes

| Feature | Clang Status | MSVC Status | Notes |
|---------|-------------|-------------|-------|
| `std::expected` | Stable (libc++ 16+) | Stable (VS 2022 17.4+) | Already in use as `Result<T>` |
| `std::flat_set` / `std::flat_map` | libc++ 18+ | VS 2022 17.6+ | Verify with current toolchain version |
| Concepts / `std::derived_from` | Stable | Stable | Already in use (`OpaqueHandleTag`) |
| Deducing `this` | Clang 18+ | VS 2022 17.2+ | Verify codegen quality |
| `std::ranges::to<>` | libc++ 17+ | VS 2022 17.6+ | Verify availability |
| `std::optional<T>` | Stable | Stable | Universally available |
| Designated initialisers | Stable | Stable | Already in use throughout |

**Recommendation:** Before using `std::flat_set`/`std::flat_map` and `std::ranges::to<>`, verify they compile cleanly on the project's Clang version with libc++. If unavailable, `std::vector` + `std::ranges::sort` (for flat set) and manual `push_back` loops (for ranges::to) are trivial fallbacks. `std::expected`, concepts, and designated initialisers are already proven in the codebase.

---

## Sources

All patterns derived from:
- C++23 Standard (ISO/IEC 14882:2024) -- language features
- Existing Wayfinder codebase patterns (`Result<T>`, `Handle<T>`, `OpaqueHandleTag` concept, `SubsystemCollection`, `SystemRegistrar` topological sort, `GameplayTag`, `InternedString`)
- Wayfinder v2 architecture plans (`application_architecture_v2.md`, `game_framework.md`, `application_migration_v2.md`)
- Engine design principles from `.github/copilot-instructions.md`

No external libraries recommended. All patterns are implementable with the C++23 standard library and existing engine primitives.
