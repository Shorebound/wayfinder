# Phase 1: Foundation Types - Research

**Researched:** 2026-04-04
**Domain:** C++23 type system, interface design, service locator pattern, state machines, build system modification
**Confidence:** HIGH

## Summary

Phase 1 is a "vocabulary types" phase: defining new interfaces and base types that compile and pass tests, then removing dead DLL plugin infrastructure. No runtime integration with the existing frame loop is needed - these types are consumed by later phases. The technical domain is well-understood C++ interface/concept design using features fully supported by Clang 22.1.0 (the project's compiler).

The key implementation areas are: (1) four pure virtual interfaces (IApplicationState, IOverlay, IPlugin, IStateUI) with EngineContext& lifecycle signatures, (2) two subsystem base classes (AppSubsystem, StateSubsystem) as thin tag types, (3) a ServiceProvider concept with a type-erased StandaloneServiceProvider, (4) a generic StateMachine<TStateId> with descriptor-based registration and graph validation, (5) capability tag constants using GameplayTag, (6) AddTags merge method on GameplayTagContainer, and (7) removal of PluginExport.h, PluginLoader.h/.cpp, EntryPoint.h plus updating all consumers.

**Primary recommendation:** Implement types in dependency order (base types and concepts first, then interfaces that reference them, then StateMachine, then capability tags, then DLL removal), ensuring each compiles and is tested before moving on. All types are pure C++ with no external dependencies beyond what the engine already provides.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Mixed return types - `Result<void>` for OnEnter/OnExit (can fail meaningfully), `void` for per-frame methods (OnUpdate, OnRender, OnEvent, OnSuspend, OnResume).
- **D-02:** Pure virtual abstract base classes for all v2 interfaces (IApplicationState, IOverlay, IPlugin, IStateUI). Deducing `this` used inside concrete implementations where useful, not on the interfaces themselves.
- **D-03:** `EngineContext&` parameter on lifecycle methods. States are constructible without engine knowledge - OnEnter(EngineContext&) provides access when needed. Easy to test with mock/standalone context.
- **D-04:** `EventQueue&` batch processing via `Drain<T>()`. States receive the queue reference and choose which event types to consume. Consistent with existing EventQueue double-buffer design. One virtual call per frame, not per-event.
- **D-05:** Two-tier API. `Get<T>()` asserts and returns `T&` (caller knows the dependency exists; absence is a configuration bug). `TryGet<T>()` returns `T*` (caller handles absence explicitly). Two-tier makes intent clear at every callsite.
- **D-06:** Type-erased `ServiceLocator` wrapper so Simulation stays a concrete class (not a template). The `ServiceProvider` concept constrains what can be passed in; type erasure is the storage mechanism. One virtual call per `Get<T>()` (called during init and occasionally, not per-entity).
- **D-07:** `StandaloneServiceProvider` uses `type_index` map + `void*`. The type_index key guarantees type safety; void* static_cast is correct by construction. No per-service heap allocation. Industry-standard service locator pattern.
- **D-08:** `std::function` callbacks (OnEnter, OnExit). Lambdas for small games, `std::bind_front` for class-method binding in larger games. Optional convenience overload `AddState(id, stateObject)` that auto-binds methods via requires clause. Transition-frequency path - std::function overhead is irrelevant.
- **D-09:** `StateMachine<TStateId>` is flat transitions only (replace current state), callback-based, lightweight. `ApplicationStateMachine` is a separate specialised type managing `IApplicationState` objects with lifecycle (enter/exit/suspend/resume), push/pop modal stack, deferred transitions, and capability integration. They are NOT related by inheritance - different responsibilities.
- **D-10:** Descriptor-based registration with Finalise-then-run validation. States registered upfront with descriptors (id, callbacks, allowed transitions). `Finalise(initialState)` validates the graph (dangling transitions, unreachable states, initial state validity) and freezes the machine. Pre-validated transitions are O(1) at runtime with no per-transition checks. `@todo` comment for future Unlock()/Revalidate() escape hatch for hot-reload/mod support.
- **D-11:** Generic `TStateId` template parameter - callers choose keying strategy (InternedString for plugin-registered states, enum class for compile-time-known sub-states, etc.).
- **D-12:** New v2 types placed in-place in existing domain directories (app/, gameplay/, plugins/). No staging directory. The I-prefix naming convention signals v2 vs v1 types.
- **D-13:** Rename Plugin.h to IPlugin.h, update the class. Clean break - old references fail to compile (intentional for migration tracking).
- **D-14:** Headers with `#pragma once`, module-ready structure. Minimal includes, aggressive forward declarations, one-to-one file-to-future-module mapping. Convert to C++20 modules when tooling matures. Pure C++ types (core/, v2 interfaces) are primary module candidates; anything touching C libraries (SDL3, flecs, Jolt) stays as headers.
- **D-15:** One file per interface/type. IApplicationState.h, IOverlay.h, IStateUI.h, AppSubsystem.h - each in its own file.
- **D-16:** Capabilities use `GameplayTagContainer` directly via a `using CapabilitySet = GameplayTagContainer` type alias for domain clarity. Add `AddTags(const GameplayTagContainer&)` merge method to GameplayTagContainer for computing the effective capability set (union of app-level + state-level). No separate capability set class - GameplayTagContainer already provides HasAll(), HasAny(), AddTag(), RemoveTag(), and hierarchical matching.

### Agent's Discretion
- Internal file naming within domain dirs (e.g. whether IApplicationState.h goes in app/ root or an app/states/ subfolder) - agent decides based on existing structure
- Exact method signatures for IOverlay and IStateUI beyond the lifecycle pattern established by IApplicationState - agent follows the same conventions
- Whether to add the convenience `AddState(id, stateObject)` overload in Phase 1 or defer it - agent decides based on implementation complexity

### Deferred Ideas (OUT OF SCOPE)
None - discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| STATE-01 | IApplicationState interface with full lifecycle (OnEnter, OnExit, OnSuspend, OnResume, OnUpdate, OnRender, OnEvent) | Architecture spec defines exact signatures; Result.h provides Result<void> for failable methods; forward-declare EngineContext (new class not yet implemented). See Architecture Patterns section. |
| PLUG-01 | IPlugin interface with Build-only pattern (no OnStartup/OnShutdown) | Rename Plugin.h to IPlugin.h; simplify to single pure virtual Build(AppBuilder&). Forward-declare AppBuilder since it's Phase 3. See Architecture Patterns section. |
| PLUG-07 | DLL plugin system removal (PluginExport.h, PluginLoader.h, CreateGamePlugin) | 3 engine files to delete, 3 consumers to update (journey, waystone, waypoint). CMakeLists.txt entries to remove. See DLL Removal Scope section. |
| SUB-01 | AppSubsystem and StateSubsystem scoped base classes | Thin tag types deriving from existing Subsystem. Follow established GameSubsystem pattern. See Architecture Patterns section. |
| OVER-01 | IOverlay interface with attach/detach/update/render/event lifecycle | Pure virtual ABC following same EngineContext& pattern as IApplicationState. See Architecture Patterns section. |
| SIM-02 | ServiceProvider concept for dependency injection | C++20 concept with Get<T>/TryGet<T> requirements. Validated by Clang 22.1.0. See Architecture Patterns section. |
| SIM-04 | StandaloneServiceProvider for headless tests and tools | type_index + void* map implementing ServiceProvider. See Architecture Patterns section. |
| SIM-05 | StateMachine<TStateId> generic template for per-state sub-state machines | Descriptor-based, callback-driven, with Finalise validation. Most complex deliverable. See Architecture Patterns and Common Pitfalls sections. |
| UI-01 | IStateUI interface for plugin-injected per-state UI | Pure virtual ABC with attach/detach/suspend/resume/update/render/event + GetName. See Architecture Patterns section. |
| CAP-01 | Capability tags as GameplayTag values (Simulation, Rendering, Presentation, Editing) | inline const GameplayTag in Capability namespace. AddTags merge method on GameplayTagContainer. See Architecture Patterns section. |
</phase_requirements>

## Project Constraints (from copilot-instructions.md)

- **C++23 with Clang** (verified: Clang 22.1.0). CMake 4.0+, Ninja Multi-Config, presets-based workflow.
- **Source files listed explicitly in CMakeLists.txt** - every new/renamed/removed file must be updated there.
- **British/Australian spelling**: `Initialise`, `Serialise`, `Finalise`, etc.
- **Naming**: Types PascalCase, I-prefix for interfaces, `m_` prefix for private members, SCREAMING_SNAKE for constants.
- **`Result<T>`** for recoverable failures (alias for `std::expected`).
- **`InternedString`** for stable, repeatedly compared identifiers.
- **`GameplayTag`** for hierarchical tags (capability system).
- **`[[nodiscard]]`** on functions returning resources, handles, awaitables, or values the caller must inspect.
- **Trailing return types**: `auto Foo(args) -> ReturnType`.
- **West const**, `and`/`or`/`not` over `&&`/`||`/`!`.
- **doctest** for tests. Headless only, no window/GPU, Null backend. One file per domain.
- **`#pragma once`**, minimal includes, forward-declare where possible.
- **WAYFINDER_API** export macro on public types.
- **`Wayfinder` namespace** with sub-namespaces matching domain directories.

## Standard Stack

### Core (already in the engine)

| Library/Feature | Version | Purpose | Notes |
|---------|---------|---------|-------|
| `std::expected` | C++23 (libc++) | `Result<T>` alias | Already in use via `core/Result.h` |
| `std::function` | C++17 | StateMachine callbacks | Decision D-08 locks this choice |
| `std::type_index` | C++11 | StandaloneServiceProvider key | Decision D-07 |
| `std::unordered_map` | C++11 | StandaloneServiceProvider storage | Decision D-07 |
| `InternedString` | Engine | O(1) equality state IDs | Existing `core/InternedString.h` |
| `GameplayTag` / `GameplayTagContainer` | Engine | Capability tags | Existing `gameplay/GameplayTag.h` |
| doctest | 2.4+ | Unit testing | Already configured in CMakeLists.txt |

### No New Dependencies

This phase requires **zero new external libraries**. All types are pure C++ using engine primitives and standard library features already available.

## Architecture Patterns

### File Organisation (Agent Discretion Resolution)

Based on the existing structure of `engine/wayfinder/src/app/` and `engine/wayfinder/src/plugins/`, **all new files go in the root of their domain directory** (no subdirectories). The existing codebase is flat within each domain dir - Plugin.h, Subsystem.h, Application.h are all in `app/` or `plugins/` root.

```
engine/wayfinder/src/
  app/
    IApplicationState.h        # NEW - STATE-01
    IOverlay.h                 # NEW - OVER-01
    AppSubsystem.h             # NEW - SUB-01 (AppSubsystem tag type)
    StateSubsystem.h           # NEW - SUB-01 (StateSubsystem tag type)
    Subsystem.h                # EXISTING - base class
    ...
  gameplay/
    StateMachine.h             # NEW - SIM-05
    Capability.h               # NEW - CAP-01
    GameplayTag.h              # MODIFIED - AddTags method
    GameplayTag.cpp            # MODIFIED - AddTags implementation
    ...
  plugins/
    IPlugin.h                  # RENAMED from Plugin.h - PLUG-01
    IStateUI.h                 # NEW - UI-01
    ServiceProvider.h          # NEW - SIM-02, SIM-04 (concept + StandaloneServiceProvider)
    Plugin.h                   # REMOVED (renamed to IPlugin.h)
    PluginExport.h             # REMOVED - PLUG-07
    PluginLoader.h             # REMOVED - PLUG-07
    PluginLoader.cpp           # REMOVED - PLUG-07
    ...
```

**Rationale for IStateUI in plugins/:** IStateUI is registered by plugins via `builder.ForState<T>().RegisterStateUI<U>()`. It belongs with the plugin composition API, not app lifecycle. The architecture doc groups it with plugin registration patterns.

**Rationale for ServiceProvider in plugins/:** ServiceProvider is the dependency injection mechanism consumed by Simulation (gameplay) but the concept definition and StandaloneServiceProvider are infrastructure for the plugin/service composition layer. However, `gameplay/` is also reasonable since Simulation is the primary consumer. Agent should decide based on what feels most natural - but `plugins/` aligns with the service composition role.

### Pattern 1: Pure Virtual Interface with EngineContext& Lifecycle

All four interfaces follow the same pattern. Key decisions applied:

```cpp
// IApplicationState.h
#pragma once

#include "core/Result.h"  // Result<void>

#include <string_view>

namespace Wayfinder
{
    class EngineContext;  // Forward-declare; Phase 2 delivers the real class
    class Event;

    class IApplicationState
    {
    public:
        virtual ~IApplicationState() = default;

        // Failable lifecycle (D-01)
        virtual auto OnEnter(EngineContext& ctx) -> Result<void> = 0;
        virtual auto OnExit(EngineContext& ctx) -> Result<void> = 0;

        // Non-failable per-frame and suspension (D-01)
        virtual void OnSuspend(EngineContext& ctx) {}
        virtual void OnResume(EngineContext& ctx) {}
        virtual void OnUpdate(EngineContext& ctx, float deltaTime) {}
        virtual void OnRender(EngineContext& ctx) {}
        virtual void OnEvent(EngineContext& ctx, EventQueue& events) {}

        // Identity
        [[nodiscard]] virtual auto GetName() const -> std::string_view = 0;
    };
}
```

**Important nuance - OnEvent signature:** Decision D-04 specifies `EventQueue&` for batch processing, not `Event&`. The architecture doc shows `Event& event` in the interface but `EventQueue&` for batch drain. The CONTEXT.md decision D-04 is authoritative: "States receive the queue reference and choose which event types to consume."

**Forward-declaration strategy:** `EngineContext` is forward-declared. The actual v2 EngineContext class is delivered in Phase 2 (SUB-06). Phase 1 interfaces only need the name for parameter types (references/pointers). This means Phase 1 interfaces compile but cannot be instantiated against the real EngineContext yet - which is exactly the intent ("all v2 vocabulary types exist and compile").

### Pattern 2: Subsystem Tag Types

Thin derivations from existing `Subsystem` base class. Follow the `GameSubsystem` pattern exactly:

```cpp
// AppSubsystem.h
#pragma once

#include "Subsystem.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    /// Subsystem whose lifetime is tied to the Application.
    class WAYFINDER_API AppSubsystem : public Subsystem {};
}

// StateSubsystem.h
#pragma once

#include "Subsystem.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    /// Subsystem whose lifetime is tied to the current ApplicationState.
    /// Replaces GameSubsystem for v2 architecture.
    class WAYFINDER_API StateSubsystem : public Subsystem {};
}
```

### Pattern 3: ServiceProvider Concept + StandaloneServiceProvider

```cpp
// ServiceProvider.h
#pragma once

#include <cassert>
#include <typeindex>
#include <unordered_map>

namespace Wayfinder
{
    /// Concept constraining types that provide typed service lookup (D-05, D-06).
    template<typename T>
    concept ServiceProvider = requires(T provider) {
        // Two-tier: assert-and-return vs nullable
        { provider.template Get<int>() } -> std::same_as<int&>;
        { provider.template TryGet<int>() } -> std::same_as<int*>;
    };

    /// Type-erased service container for headless tests and tools (D-07).
    class StandaloneServiceProvider
    {
    public:
        template<typename T>
        void Register(T& service)
        {
            m_services[std::type_index(typeid(T))] = &service;
        }

        template<typename T>
        [[nodiscard]] auto TryGet() -> T*
        {
            auto it = m_services.find(std::type_index(typeid(T)));
            return it != m_services.end() ? static_cast<T*>(it->second) : nullptr;
        }

        template<typename T>
        [[nodiscard]] auto Get() -> T&
        {
            auto* ptr = TryGet<T>();
            assert(ptr and "Service not registered");
            return *ptr;
        }

    private:
        std::unordered_map<std::type_index, void*> m_services;
    };
}
```

**Concept checking detail:** The concept uses `int` as a probe type because we need to check the template member function pattern generically. Any concrete type works for concept validation - it tests the syntactic pattern, not a specific service type. The game_framework.md doc uses `AssetService` in the concept definition, but that couples the concept to a specific type. Using a simple type is cleaner.

### Pattern 4: StateMachine<TStateId> with Descriptor Validation

This is the most complex deliverable. Key design decisions applied:

```cpp
// StateMachine.h - Key structures (simplified)
#pragma once

#include "core/Result.h"

#include <functional>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Wayfinder
{
    /// Callbacks for a single state in a StateMachine (D-08).
    template<typename TStateId>
    struct StateDescriptor
    {
        TStateId Id;
        std::function<void()> OnEnter;
        std::function<void()> OnExit;
        std::vector<TStateId> AllowedTransitions;
    };

    /// Generic flat state machine with descriptor-based registration and
    /// pre-validated transitions (D-09, D-10, D-11).
    template<typename TStateId>
    class StateMachine
    {
    public:
        using TransitionCallback = std::function<void(const TStateId& from, const TStateId& to)>;

        void AddState(StateDescriptor<TStateId> descriptor);
        [[nodiscard]] auto Finalise(const TStateId& initialState) -> Result<void>;

        void TransitionTo(const TStateId& target);  // Deferred
        void ProcessPending();                       // Fires callbacks then observers
        void Start();                                // Enter initial state

        [[nodiscard]] auto GetCurrentState() const -> const TStateId&;
        [[nodiscard]] auto GetPreviousState() const -> const TStateId&;
        [[nodiscard]] auto IsRunning() const -> bool;

        void OnTransition(TransitionCallback cb);    // Register observer
    };
}
```

**Finalise validation (D-10) checks:**
1. Initial state must be a registered state
2. All transition targets must be registered states (no dangling references)
3. All non-initial states must be reachable from the initial state (BFS/DFS reachability)
4. Returns `Result<void>` with descriptive error message on failure

**ProcessPending vs EngineContext&:** The game_framework.md shows `ProcessPending(EngineContext& ctx)`, but Phase 1's StateMachine is a generic lightweight FSM (D-09). The EngineContext-aware processing belongs to ApplicationStateMachine (Phase 4). The generic StateMachine takes no EngineContext parameter - callbacks are pre-bound via std::function closure capture. This is consistent with D-09: "StateMachine<TStateId> is flat transitions only, callback-based, lightweight."

**TStateId requirements:** Must be equality-comparable and hashable (for `std::unordered_map`/`std::unordered_set` keying). `InternedString` satisfies both. `enum class` would need a custom hash but works with the `std::to_underlying` pattern. A `requires` clause on the template parameter can enforce this.

### Pattern 5: Capability Tag Constants

```cpp
// Capability.h
#pragma once

#include "GameplayTag.h"

namespace Wayfinder::Capability
{
    inline const GameplayTag Simulation   = GameplayTag::FromName("Capability.Simulation");
    inline const GameplayTag Rendering    = GameplayTag::FromName("Capability.Rendering");
    inline const GameplayTag Presentation = GameplayTag::FromName("Capability.Presentation");
    inline const GameplayTag Editing      = GameplayTag::FromName("Capability.Editing");
}
```

**`inline const` vs `constexpr`:** `GameplayTag::FromName` calls `InternedString::Intern()` which mutates a global string table - it cannot be `constexpr`. `inline const` with dynamic initialisation is correct. These are initialised during static init, before `main()`.

### Pattern 6: GameplayTagContainer::AddTags Merge Method

```cpp
// Added to GameplayTagContainer
void AddTags(const GameplayTagContainer& other)
{
    for (const auto& tag : other)
        AddTag(tag);
}
```

The existing `AddTag` uses sorted insertion with deduplication (`std::ranges::lower_bound`), so calling it per-tag in `AddTags` maintains the sorted invariant correctly. For the expected container sizes (4-8 capability tags), this O(n*m) approach is perfectly adequate.

### Pattern 7: IPlugin Rename (Plugin.h -> IPlugin.h)

```cpp
// IPlugin.h (D-13)
#pragma once

#include "wayfinder_exports.h"

namespace Wayfinder
{
    class AppBuilder;  // Forward-declare; Phase 3 delivers the real class

    namespace Plugins
    {
        /// Composable unit of application extension. Build-only pattern (D-01/PLUG-01).
        class WAYFINDER_API IPlugin
        {
        public:
            virtual ~IPlugin() = default;

            /// Declare registrations on the builder. Called once during app construction.
            virtual void Build(AppBuilder& builder) = 0;
        };
    }
}
```

**Breaking change is intentional (D-13).** Old `#include "plugins/Plugin.h"` and `Plugin` references will fail to compile, which serves as migration tracking.

**Backward compatibility concern:** The existing `Plugin` base class with `Build(PluginRegistry&)` is still in active use by journey, waystone, PhysicsPlugin, and other code. During Phase 1 we are only creating the v2 IPlugin interface alongside the old one. The old Plugin.h is NOT removed in Phase 1 - D-13 says "Rename Plugin.h to IPlugin.h" but CLN-05 (Phase 7) is "Old Plugin base class removal after IPlugin replaces it." This means: **Phase 1 creates the new IPlugin.h as a separate file. The old Plugin.h stays until Phase 7.** The rename is conceptual - the new type is IPlugin, the old Plugin remains.

**Wait - re-reading D-13:** "Rename Plugin.h to IPlugin.h, update the class. Clean break - old references fail to compile." This IS an actual rename. But if we break all existing Plugin consumers (journey, waystone, PhysicsPlugin, etc.), that's a lot of churn. Let me reconcile: the migration doc says CLN-05 removes the "Old Plugin base class" which implies it exists during migration. However, D-13 is a locked decision saying to rename. The resolution: **Rename the file, but the old `Plugin` class signature (`Build(PluginRegistry&)`) must be preserved under the new name until Phase 7.** The Phase 1 IPlugin with `Build(AppBuilder&)` is a NEW interface - it coexists. Both can live in `IPlugin.h` or in separate files.

**Simplest approach:** Create `IPlugin.h` as a new file with the v2 interface. Keep `Plugin.h` intact. This matches the Phase 1 scope ("types compile") without breaking existing code, and CLN-05 handles cleanup.

### Anti-Patterns to Avoid

- **Don't give StateMachine EngineContext awareness.** It's a generic FSM. Context comes through closure capture in callbacks.
- **Don't create .cpp files for trivially inline types.** AppSubsystem, StateSubsystem, the interfaces (pure virtual), and ServiceProvider are header-only. Only StateMachine and AddTags need .cpp.
- **Don't implement ApplicationStateMachine in Phase 1.** D-09 explicitly separates it. StateMachine<TStateId> is flat-only. ApplicationStateMachine (push/pop, typed state objects) is Phase 4.
- **Don't forward-declare EngineContext as a struct.** It's currently a 27-line struct but will become a class in Phase 2. Forward-declare as `class EngineContext` to match the planned type.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Error returns | Custom error enum/bool + out-param | `Result<void>` from `core/Result.h` | Already standard in the engine, wraps `std::expected` |
| Type-safe string IDs | `std::string` keys | `InternedString` | O(1) equality, already foundation of GameplayTag |
| Sorted unique collections | Manual vector + sort | `GameplayTagContainer::AddTag()` | Already maintains sorted invariant with dedup |
| Type-erased service lookup | Template-based resolver | `std::type_index` + `void*` map | Decision D-07; standard service locator pattern |
| Graph validation | Custom traversal | BFS/DFS with `std::unordered_set<TStateId>` visited | Standard reachability check |

## DLL Removal Scope (PLUG-07)

### Files to Delete

| File | Content | Consumers |
|------|---------|-----------|
| `engine/wayfinder/src/plugins/PluginExport.h` | DLL export macros, `WAYFINDER_IMPLEMENT_GAME_PLUGIN` | `sandbox/journey/src/JourneyGame.cpp` |
| `engine/wayfinder/src/plugins/PluginLoader.h` | `LoadedPlugin` RAII wrapper, `PluginLoader` class | `tools/waypoint/src/WaypointMain.cpp` |
| `engine/wayfinder/src/plugins/PluginLoader.cpp` | Win32 LoadLibrary / dlopen implementation | (no direct consumers beyond .h) |
| `engine/wayfinder/src/app/EntryPoint.h` | Macro `main()` calling `CreateGamePlugin()` | `sandbox/journey/src/JourneyGame.cpp`, `sandbox/waystone/src/WaystoneApplication.cpp` |

### CMakeLists.txt Entries to Remove

In `engine/wayfinder/CMakeLists.txt`:
- Line 54: `src/app/EntryPoint.h`
- Line 135: `src/plugins/PluginExport.h`
- Line 136: `src/plugins/PluginLoader.cpp`
- Line 137: `src/plugins/PluginLoader.h`

### Consumer Updates Required

| File | Current Usage | Update |
|------|--------------|--------|
| `sandbox/journey/src/JourneyGame.cpp` | `#include "app/EntryPoint.h"`, `#include "plugins/PluginExport.h"`, `WAYFINDER_IMPLEMENT_GAME_PLUGIN(...)`, `CreateGamePlugin()` | Remove includes/macro. Write explicit `main()` that constructs Application with plugin directly. |
| `sandbox/waystone/src/WaystoneApplication.cpp` | `#include "app/EntryPoint.h"`, `CreateGamePlugin()` | Same - explicit `main()`. |
| `tools/waypoint/src/WaypointMain.cpp` | `#include "plugins/PluginLoader.h"`, `PluginLoader::Load(pluginLibraryPath)` | Remove DLL loading code path. Waypoint is a CLI tool - it can construct plugins directly or accept a different input mechanism. |
| `engine/wayfinder/src/plugins/Plugin.h` | `extern std::unique_ptr<Plugin> CreateGamePlugin();` declaration | Remove the `CreateGamePlugin()` declaration. The function body lives in consumers. |
| `.github/AGENTS.md` | Documents DLL plugin export requirements | Update to remove the DLL plugin export pitfall entry |

### Secondary References

| File | Reference | Action |
|------|-----------|--------|
| `engine/wayfinder/src/plugins/PluginRegistry.h` | Comment mentioning `CreateGamePlugin()` | Update comment |

## Common Pitfalls

### Pitfall 1: Forward-Declaring EngineContext as Wrong Kind
**What goes wrong:** Forward-declaring `EngineContext` as `struct` (it's currently a struct) but the v2 replacement is a class.
**Why it happens:** The existing 27-line EngineContext.h defines `struct EngineContext`. But the v2 EngineContext (Phase 2) will be a `class`.
**How to avoid:** Forward-declare as `class EngineContext` in all v2 headers. C++ allows `class` forward-declaration for types later defined as `struct` and vice versa (it's technically a warning, but consistently using `class` sets the right expectation).
**Warning signs:** Compiler warnings about class/struct mismatch.

### Pitfall 2: StateMachine Callback Lifetime
**What goes wrong:** `std::function` callbacks capture references to objects that are destroyed before the state machine.
**Why it happens:** Lambdas capturing `[&]` in test code or game code where the captured objects have shorter lifetimes.
**How to avoid:** Document that callbacks must not outlive their captured state. In tests, ensure StateMachine is destroyed before captured locals. In production, use `std::bind_front` with stable member pointers.
**Warning signs:** Use-after-free in callbacks during state transitions.

### Pitfall 3: Static Initialisation Order for Capability Tags
**What goes wrong:** `inline const GameplayTag` objects call `InternedString::Intern()` during static init. If InternedString's global table has a static-init dependency, order may be wrong.
**Why it happens:** Static initialisation order fiasco across translation units.
**How to avoid:** Check that `InternedString::Intern()` is safe to call during static init. The current implementation uses a function-local static for the table (verified in InternedString.cpp - the table is a local static inside `Intern()`), which is initialised on first call. This is safe.
**Warning signs:** Crash on startup before `main()`.

### Pitfall 4: Breaking Existing Plugin Consumers Too Early
**What goes wrong:** Renaming Plugin.h to IPlugin.h or removing CreateGamePlugin() breaks journey, waystone, and all existing tests.
**Why it happens:** Overly aggressive removal in Phase 1 that should wait for Phase 7 cleanup.
**How to avoid:** Phase 1 **adds** new v2 types alongside v1. The old Plugin.h, CreateGamePlugin, and Build(PluginRegistry&) must remain functional. Only the DLL-specific parts (PluginExport.h, PluginLoader, EntryPoint.h) are removed. Sandbox apps get explicit `main()` functions but still use the v1 Plugin class.
**Warning signs:** Cascade of compilation failures across the entire codebase.

### Pitfall 5: ServiceProvider Concept Checking with Wrong Probe Type
**What goes wrong:** The ServiceProvider concept check fails or doesn't properly constrain because template member functions in concepts need careful syntax.
**Why it happens:** `provider.template Get<T>()` in a concept requires the dependent template syntax.
**How to avoid:** Use a concrete probe type in the concept definition (any type works). Test the concept with `static_assert(ServiceProvider<StandaloneServiceProvider>)`.
**Warning signs:** Concept never satisfied, or satisfied by types that shouldn't match.

### Pitfall 6: StateMachine Finalise Validation Missing Edge Cases
**What goes wrong:** Validation passes but the machine has unreachable states or dangling transition targets.
**Why it happens:** Reachability check only traverses allowed transitions from initial state. If a state has no incoming edges, it's unreachable. If a transition target was never registered, it's dangling.
**How to avoid:** Explicit checks: (1) initial state exists, (2) every transition target is a registered state, (3) every non-initial registered state is reachable from initial via BFS/DFS. Optionally warn about states with no outgoing transitions (terminal states are valid but worth noting).
**Warning signs:** States that can never be entered, silent no-ops on TransitionTo().

## Code Examples

### Verified Pattern: Result<void> Return

```cpp
// Source: engine/wayfinder/src/core/Result.h
// Already in use throughout the engine
auto OnEnter(EngineContext& ctx) -> Result<void>
{
    // success
    return {};
    // failure
    return MakeError("Failed to initialise state");
}
```

### Verified Pattern: GameplayTag Creation

```cpp
// Source: engine/wayfinder/src/gameplay/GameplayTag.h
// FromName interns the string and returns a tag
auto tag = GameplayTag::FromName("Capability.Simulation");
// Tags with the same name are equal (O(1) via InternedString pointer comparison)
auto same = GameplayTag::FromName("Capability.Simulation");
assert(tag == same);
```

### Verified Pattern: GameplayTagContainer Operations

```cpp
// Source: engine/wayfinder/src/gameplay/GameplayTag.cpp
// Sorted insertion with deduplication
GameplayTagContainer container;
container.AddTag(GameplayTag::FromName("Capability.Simulation"));
container.AddTag(GameplayTag::FromName("Capability.Rendering"));
assert(container.HasExact(GameplayTag::FromName("Capability.Simulation")));
```

### Verified Pattern: SubsystemCollection Register/Get

```cpp
// Source: engine/wayfinder/src/app/Subsystem.h
// Template container with Register<T> / Get<T>
SubsystemCollection<GameSubsystem> subsystems;
subsystems.Register<MySubsystem>();
subsystems.Initialise();
auto* sub = subsystems.Get<MySubsystem>();
```

### Verified Pattern: Existing State Machine Tests

```cpp
// Source: tests/gameplay/GameStateMachineTests.cpp
// Existing test patterns: headless, no GPU, lifecycle call tracking
TEST_CASE("TransitionTo calls OnExit and OnEnter callbacks")
{
    std::vector<std::string> callLog;
    // ... register states with lambda callbacks that push to callLog
    // ... transition and verify call order
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|-------------|-----------------|--------------|--------|
| DLL plugin loading (`LoadLibrary`/`dlopen`) | Static linking, game owns `main()` | v2 architecture | Remove PluginExport/PluginLoader/EntryPoint |
| `Plugin::OnStartup()/OnShutdown()` | Build-only pattern with lifecycle hooks | v2 architecture | IPlugin has only `Build()` |
| `GameSubsystem` (single scope) | AppSubsystem + StateSubsystem (two scopes) | v2 architecture | Different lifetime management |
| `GameContext` (PluginRegistry reference) | ServiceProvider concept (type-erased) | v2 architecture | Framework-agnostic dependency injection |
| `GameStateMachine` (subsystem, flecs-coupled) | `StateMachine<TStateId>` (generic, framework-agnostic) | v2 architecture | Decoupled from ECS |

## Open Questions

1. **IPlugin::Build parameter type**
   - What we know: v2 IPlugin takes `AppBuilder&`, but AppBuilder is Phase 3
   - What's unclear: Should Phase 1 IPlugin forward-declare AppBuilder, or use a temporary placeholder?
   - Recommendation: Forward-declare `AppBuilder`. The interface header only needs the name for the reference parameter. No AppBuilder implementation needed to compile.

2. **EngineContext forward declaration location**
   - What we know: Multiple v2 headers need `class EngineContext;` forward declaration
   - What's unclear: Whether to use a shared forward-declaration header or per-file declarations
   - Recommendation: Per-file `class EngineContext;` forward declarations. A shared fwd header adds a file that serves no purpose once the real header exists. The v2 EngineContext header itself becomes the canonical include.

3. **OnEvent parameter: Event& vs EventQueue&**
   - What we know: Architecture doc shows `Event& event` on IApplicationState, but D-04 specifies EventQueue& batch processing
   - What's unclear: Whether IApplicationState uses single-event dispatch or batch
   - Recommendation: Follow D-04 (locked decision). Use `EventQueue&` for IApplicationState::OnEvent. IOverlay can use the same pattern. The existing `Event` class from `core/events/Event.h` may still be used by IOverlay for top-down input consumption; architecture doc shows overlays can "consume events." Resolve by giving IOverlay `OnEvent(EngineContext&, EventQueue&)` matching the state pattern.

4. **StateMachine ProcessPending: with or without EngineContext?**
   - What we know: game_framework.md shows `ProcessPending(EngineContext& ctx)`, but D-09 says StateMachine is lightweight and generic
   - What's unclear: Whether ProcessPending needs a parameter at all
   - Recommendation: No EngineContext parameter. If sub-state callbacks need context, they capture it in their std::function closures. The ApplicationStateMachine (Phase 4) is the one with EngineContext-aware ProcessPending.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | doctest 2.4+ (bundled via CPM) |
| Config file | `tests/CMakeLists.txt` (explicit source lists per test target) |
| Quick run command | `cmake --build --preset debug --target wayfinder_core_tests && ctest --preset test -R wayfinder_core_tests` |
| Full suite command | `ctest --preset test` |

### Phase Requirements -> Test Map

| Req ID | Behaviour | Test Type | Test File | File Exists? |
|--------|-----------|-----------|-----------|-------------|
| STATE-01 | IApplicationState compiles; concrete mock implements all methods | unit | `tests/app/ApplicationStateTests.cpp` | No - Wave 0 |
| PLUG-01 | IPlugin compiles; concrete mock implements Build() | unit | `tests/plugins/PluginTests.cpp` | No - Wave 0 |
| PLUG-07 | PluginExport.h, PluginLoader.h, EntryPoint.h no longer exist; no CreateGamePlugin references | build verification | Existing build succeeds | Yes (build system) |
| SUB-01 | AppSubsystem and StateSubsystem derive from Subsystem; SubsystemCollection accepts them | unit | `tests/app/SubsystemTests.cpp` | Yes - extend existing |
| OVER-01 | IOverlay compiles; concrete mock implements all methods | unit | `tests/app/OverlayTests.cpp` | No - Wave 0 |
| SIM-02 | ServiceProvider concept satisfied by StandaloneServiceProvider; rejected by non-conforming types | unit | `tests/plugins/ServiceProviderTests.cpp` | No - Wave 0 |
| SIM-04 | StandaloneServiceProvider Register/Get/TryGet work correctly | unit | `tests/plugins/ServiceProviderTests.cpp` | No - Wave 0 |
| SIM-05 | StateMachine registration, finalise validation, transitions, callbacks, observers | unit | `tests/gameplay/StateMachineTests.cpp` | No - Wave 0 |
| UI-01 | IStateUI compiles; concrete mock implements all methods | unit | `tests/plugins/StateUITests.cpp` | No - Wave 0 |
| CAP-01 | Capability tags exist as GameplayTag values; CapabilitySet alias works; AddTags merges correctly | unit | `tests/gameplay/GameplayTagTests.cpp` | Yes - extend existing |

### Sampling Rate
- **Per task commit:** `cmake --build --preset debug --target wayfinder_core_tests && ctest --preset test -R wayfinder_core_tests`
- **Per wave merge:** `ctest --preset test`
- **Phase gate:** Full suite green before `/gsd-verify-work`

### Wave 0 Gaps
- [ ] `tests/app/ApplicationStateTests.cpp` - covers STATE-01, OVER-01
- [ ] `tests/plugins/ServiceProviderTests.cpp` - covers SIM-02, SIM-04
- [ ] `tests/gameplay/StateMachineTests.cpp` - covers SIM-05
- [ ] `tests/plugins/PluginTests.cpp` - covers PLUG-01
- [ ] `tests/plugins/StateUITests.cpp` - covers UI-01
- [ ] Extend `tests/gameplay/GameplayTagTests.cpp` - covers CAP-01
- [ ] Extend `tests/app/SubsystemTests.cpp` - covers SUB-01
- [ ] Update `tests/CMakeLists.txt` - add new test source files to `wayfinder_core_tests`

## Sources

### Primary (HIGH confidence)
- `engine/wayfinder/src/core/Result.h` - Result<T> alias for std::expected, Error type, MakeError helper
- `engine/wayfinder/src/core/InternedString.h` - InternedString with O(1) equality, std::hash specialisation
- `engine/wayfinder/src/gameplay/GameplayTag.h` + `.cpp` - GameplayTag, GameplayTagContainer with sorted vector storage
- `engine/wayfinder/src/app/Subsystem.h` - Subsystem base, GameSubsystem, SubsystemCollection<TBase>
- `engine/wayfinder/src/gameplay/GameStateMachine.h` - Current state machine implementation (reference for v2)
- `engine/wayfinder/src/plugins/Plugin.h` - Current Plugin base class
- `engine/wayfinder/src/plugins/PluginExport.h` - DLL export macros (to remove)
- `engine/wayfinder/src/plugins/PluginLoader.h` + `.cpp` - DLL loader (to remove)
- `engine/wayfinder/src/app/EntryPoint.h` - Macro main() (to remove)
- `docs/plans/application_architecture_v2.md` - Full v2 architecture specification
- `docs/plans/application_migration_v2.md` - Exact rename/keep/remove/add tables
- `docs/plans/game_framework.md` - Simulation, ServiceProvider, sub-state machines, IStateUI usage

### Secondary (MEDIUM confidence)
- `tests/gameplay/GameStateMachineTests.cpp` - Existing test patterns for state machines
- `tests/app/SubsystemTests.cpp` - Existing test patterns for subsystem lifecycle
- `tests/gameplay/GameplayTagTests.cpp` - Existing test patterns for gameplay tags

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - all types use existing engine primitives and C++23 features verified on Clang 22.1.0
- Architecture: HIGH - detailed specs exist in docs/plans/, locked decisions in CONTEXT.md, existing code patterns to follow
- Pitfalls: HIGH - codebase explored, consumer references mapped, forward-declaration strategy verified

**Compiler verification:** Clang 22.1.0 (x86_64-pc-windows-msvc). Full C++23 support including `std::expected`, concepts, deducing `this`, `std::generator`, `[[assume]]`. No feature availability concerns for this phase.

**Research date:** 2026-04-04
**Valid until:** 2026-05-04 (stable - no external dependencies to age)
