# Phase 3: Plugin Composition - Research

**Researched:** 2025-07-15
**Domain:** C++23 plugin composition, typed registrar stores, builder patterns, config systems, validation
**Confidence:** HIGH

## Summary

Phase 3 transforms Wayfinder's plugin system from a simple Build-only interface with a monolithic PluginRegistry into a fully compositional architecture: AppBuilder with type-erased registrar store, plugin dependency ordering, concept-based plugin groups, lifecycle hooks, immutable AppDescriptor with processed registrar outputs, smart-accumulation validation, and a full ConfigService with 3-tier TOML config layering.

The codebase already has strong foundations for every major pattern needed. SubsystemRegistry demonstrates the `std::type_index`-keyed type-erased storage, Kahn's topological sort, and `Finalise() -> Result<void>` lifecycle. The existing registrars (State, System, Tag) define the registrar interface shape. tomlplusplus is linked and used in EngineConfig.cpp and ProjectDescriptor.cpp for TOML parsing. The primary work is composing these proven patterns into the AppBuilder/AppDescriptor/ConfigService architecture outlined in the design docs.

**Primary recommendation:** Build AppBuilder's type-erased registrar store using the same `std::type_index` + `std::unordered_map` pattern proven in SubsystemRegistry and ServiceProvider. Extract Kahn's topological sort into a shared utility for both plugin dependency ordering and the subsystem/system registrars. Use tomlplusplus's `toml::table` merging for the 3-tier config layering. Apply compiler-style error accumulation via a `ValidationResult` type that collects multiple `Error` entries.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Plugin dependencies declared via virtual `Describe()` on IPlugin returning `PluginDescriptor{.DependsOn}`. Plugin owns its dependencies internally -- no external modification mechanism. `Describe()` has a default implementation returning empty descriptor (most plugins have no deps). Aligns with SubsystemDescriptor's `.DependsOn` pattern for codebase cohesion.
- **D-02:** Plugin deps are internalised. No realistic case for external modification of plugin dependency declarations.
- **D-03:** Concept-based plugin groups. A plugin group is a struct with `Build(AppBuilder&)` that is NOT an IPlugin. Groups are transparent composition -- they don't appear in AppDescriptor as plugins. Room to grow into richer group features (conditional inclusion, platform groups) without polluting the plugin interface. `AppBuilder::AddPlugin` accepts both IPlugin-derived types and group concepts via overload/concept dispatch.
- **D-04:** Lifecycle hooks via builder lambdas: `builder.OnAppReady(lambda)`, `builder.OnStateEnter<T>(lambda)`, `builder.OnStateExit<T>(lambda)`, `builder.OnShutdown(lambda)`. IPlugin interface stays clean (Build-only + Describe). Hooks stored in registrar output, fired by Application at the correct lifecycle points.
- **D-05:** Hook points are app-level + typed state: OnAppReady, OnStateEnter<T>, OnStateExit<T>, OnShutdown. No frame-level hooks -- subsystems and render features cover per-frame work. This keeps hooks for lifecycle boundaries only.
- **D-06:** 3-tier config layering: struct defaults -> config/<key>.toml -> saved/config/<key>.toml
- **D-07:** One file per config key. `builder.LoadConfig<T>("physics")` maps to `config/physics.toml`. Only loaded if the plugin is present. Cached.
- **D-08:** Per-plugin override files in `saved/config/`. Mirrors `config/` structure.
- **D-09:** Config defaults with logging on load. `LoadConfig<T>("key")` returns `T{}` if file doesn't exist. Logs at Info level.
- **D-10:** Full ConfigService in Phase 3 scope. ConfigRegistrar (build-time collection), ConfigService (AppSubsystem, address-stable storage, TOML loading), `OnConfigReloaded()` virtual stub.
- **D-11:** Platform-conditional configuration is a future extension point. Not Phase 3 scope.
- **D-12:** Processed outputs, not frozen registrars. Each registrar has `Finalise() -> Result<OutputType>`. AppDescriptor holds the outputs. Registrar objects are destroyed after Finalise.
- **D-13:** Smart-accumulation validation (compiler-style). Within a registrar: accumulate independent errors, skip dependent checks if prerequisites fail. Across registrars: always run all registrars. Cross-registrar validation only runs if all individual registrars passed.
- **D-14:** Application owns AppDescriptor as a value member. EngineContext holds a non-owning reference.
- **D-15:** Retrofit SubsystemRegistry from frozen-registrar to processed-output pattern. `Finalise()` returns `Result<SubsystemManifest<TBase>>`. Read-only accessors move to SubsystemManifest.

### Agent's Discretion
- Internal storage for AppBuilder's type-keyed registrar container (std::unordered_map<type_index, unique_ptr<IRegistrar>> or similar)
- Exact error message formatting for validation failures
- Whether convenience methods on AppBuilder are thin inline wrappers or dispatch through a helper
- PluginDescriptor struct fields beyond DependsOn
- ConfigRegistrar internal design
- SubsystemManifest<TBase> internal storage details
- How concept dispatch for plugin groups vs IPlugin types is implemented

### Deferred Ideas (OUT OF SCOPE)
- Platform-conditional config values (D-11)
- Subsystem Describe() cohesion pass (D-16)
- config_service.md standalone document (D-17)
- User overrides persistence (console system responsibility)
- Hot-reload file watcher (interface stubbed, implementation deferred)
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| PLUG-02 | AppBuilder replacing PluginRegistry with typed registrar store | Type-erased registrar store pattern (type_index keyed map), existing registrar migration path |
| PLUG-03 | Plugin dependency declaration and topological ordering | Kahn's algorithm extraction, PluginDescriptor design, Describe() virtual method |
| PLUG-04 | Plugin group types for composing plugin sets | Concept-based dispatch pattern, transparent composition |
| PLUG-05 | AppDescriptor as read-only snapshot from Finalise() | Processed-output pattern, type-keyed output retrieval |
| PLUG-06 | Lifecycle hook registration | Builder lambda pattern, typed state hooks via template |
| CFG-01 | Per-plugin configuration replacing monolithic EngineConfig | ConfigService design, 3-tier layering, tomlplusplus merging |
| CFG-02 | AppBuilder::LoadConfig<T>(section) with cached loading | TOML file caching, struct-from-table deserialisation |
| APP-01 | Application::AddPlugin<T>() as sole public API | Application rewrite, plugin group expansion, deferred Build() |
</phase_requirements>

## Project Constraints (from copilot-instructions.md)

Key directives the planner must honour:
- **C++23 standard**, Clang, CMake 4.0+, Ninja Multi-Config
- **West const**, trailing return types, `and`/`or`/`not` keywords, British/Australian spelling
- **Result<T>** for recoverable failures (std::expected alias in core/Result.h)
- **RAII everywhere**, `[[nodiscard]]` on resource-returning functions
- **`requires` clauses** over SFINAE, named concepts for reuse
- **Designated initialisers** for aggregates/configs
- **`std::type_index`** for type-keyed storage (established pattern)
- **TOML** for hand-authored config, **JSON** for interchange/generated data
- **doctest** for testing, headless only, one file per domain
- **Source files listed explicitly** in CMakeLists.txt
- **`Wayfinder::`** namespace, sub-namespaces matching domain directories
- **InternedString** for stable, repeatedly compared identifiers
- **`enum class`** over `enum`, `std::to_underlying()` over casts
- **Modules preferred** for new subsystems (fall back to headers with `#pragma once`)
- **`auto Foo(args) -> ReturnType`** trailing return style

## Architecture Patterns

### Pattern 1: Type-Erased Registrar Store (AppBuilder core)

**What:** AppBuilder owns a `std::unordered_map<std::type_index, std::unique_ptr<IRegistrar>>` where each registrar is a strongly-typed component accessed via `builder.Registrar<ConcreteRegistrar>()`.

**Why this pattern:** Already proven in the codebase. SubsystemRegistry uses `std::unordered_map<std::type_index, size_t>` for type lookup. ServiceProvider uses `std::unordered_map<std::type_index, void*>`. The registrar store uses the same key type but owns the values via `unique_ptr<IRegistrar>` for RAII cleanup.

**Design:**

```cpp
/// Base interface for all registrars. Enables type-erased storage and
/// uniform Finalise() across heterogeneous registrar types.
class IRegistrar
{
public:
    virtual ~IRegistrar() = default;
};

/// CRTP or direct derivation by each concrete registrar.
/// Each registrar defines its own OutputType and Finalise() signature.
template<typename TOutput>
class RegistrarBase : public IRegistrar
{
public:
    using OutputType = TOutput;

    [[nodiscard]] virtual auto Finalise() -> Result<TOutput> = 0;
};
```

**AppBuilder retrieval:**

```cpp
template<typename TRegistrar>
    requires std::derived_from<TRegistrar, IRegistrar>
auto Registrar() -> TRegistrar&
{
    const auto key = std::type_index(typeid(TRegistrar));
    auto it = m_registrars.find(key);
    if (it == m_registrars.end())
    {
        auto [inserted, _] = m_registrars.emplace(
            key, std::make_unique<TRegistrar>());
        it = inserted;
    }
    return static_cast<TRegistrar&>(*it->second);
}
```

This is lazy-creation: the first plugin to request a registrar type creates it. No upfront registration of registrar types needed.

**Confidence:** HIGH - This is a direct application of patterns already in SubsystemRegistry.h and ServiceProvider.h.

### Pattern 2: Plugin Dependency Ordering via Kahn's Algorithm

**What:** Plugins declare dependencies via `Describe() -> PluginDescriptor`. AppBuilder collects descriptors, builds a dependency graph, and topologically sorts Build() calls using Kahn's algorithm.

**Why this pattern:** Kahn's algorithm is already implemented twice in the codebase (SubsystemRegistry::Finalise and SystemRegistrar::ApplyToWorld). Both implementations handle cycle detection and produce clear error messages.

**Design:**

```cpp
struct PluginDescriptor
{
    /// Human-readable name for debug output and duplicate detection.
    InternedString Name;

    /// Type indices of plugins this plugin depends on.
    /// Build() is called after all dependencies' Build() calls complete.
    std::vector<std::type_index> DependsOn;
};
```

The dependency vector uses `std::type_index` rather than string names. This matches SubsystemDescriptor's `std::vector<std::type_index> DependsOn` exactly, maintaining codebase cohesion (D-01).

**Shared utility recommendation:** Extract the Kahn's algorithm from SubsystemRegistry into a reusable utility:

```cpp
// core/TopologicalSort.h
struct TopologicalSortResult
{
    std::vector<size_t> Order;
    bool HasCycle = false;
    std::string CyclePath; // populated only if HasCycle
};

[[nodiscard]] auto TopologicalSort(
    size_t nodeCount,
    std::span<const std::vector<size_t>> adjacency
) -> TopologicalSortResult;
```

Then SubsystemRegistry, SystemRegistrar, and the new plugin ordering all use the same implementation. This reduces the three (soon four) copies of Kahn's to one.

**Confidence:** HIGH - Algorithm is already proven in codebase, pattern is well-understood.

### Pattern 3: Concept-Based Plugin Groups

**What:** Plugin groups are structs satisfying a concept, NOT derived from IPlugin (D-03). Groups call `builder.AddPlugin<T>()` inside their `Build()` method. `AppBuilder::AddPlugin` dispatches to the correct path via `if constexpr` and concept constraints.

**Design:**

```cpp
template<typename T>
concept PluginGroupType = requires(T group, AppBuilder& builder)
{
    { group.Build(builder) } -> std::same_as<void>;
} and not std::derived_from<T, IPlugin>;

template<typename T>
concept PluginType = std::derived_from<T, IPlugin>;
```

**AppBuilder dispatch:**

```cpp
template<typename T>
void AddPlugin()
{
    if constexpr (PluginType<T>)
    {
        // Register as an actual plugin - stored, ordered, built
        AddPluginImpl<T>();
    }
    else if constexpr (PluginGroupType<T>)
    {
        // Transparent expansion - group itself not tracked
        T group{};
        group.Build(*this);
    }
    else
    {
        static_assert(ALWAYS_FALSE<T>,
            "T must derive from IPlugin or satisfy PluginGroupType");
    }
}
```

**Why `if constexpr` over overloads:** A single `AddPlugin<T>()` entry point is cleaner than two overloads. The `static_assert` with `ALWAYS_FALSE<T>` in the else branch catches misuse at compile time (per copilot-instructions.md patterns).

**Bevy reference:** Bevy separates `Plugin` (trait with `build(&self, app: &mut App)`) from `PluginGroup` (trait with `build(self) -> PluginGroupBuilder`). Groups compose via `PluginGroupBuilder` which tracks ordering and enable/disable. Wayfinder's concept-based approach achieves the same separation with C++23 concepts instead of Rust traits, and is simpler since we don't need the PluginGroupBuilder's enable/disable/reorder machinery in Phase 3.

**Confidence:** HIGH - Concept + `if constexpr` dispatch is idiomatic C++23.

### Pattern 4: Processed Outputs and AppDescriptor

**What:** Each registrar's `Finalise()` returns a `Result<OutputType>`. AppBuilder collects all outputs into AppDescriptor, a read-only aggregate queryable by output type. Registrar objects are destroyed after Finalise (D-12).

**Design:**

```cpp
class AppDescriptor
{
public:
    template<typename TOutput>
    auto Get() const -> const TOutput&
    {
        const auto key = std::type_index(typeid(TOutput));
        auto it = m_outputs.find(key);
        WAYFINDER_ASSERT(it != m_outputs.end(),
            "AppDescriptor::Get<{}> - output not found", typeid(TOutput).name());
        return *static_cast<const TOutput*>(it->second.get());
    }

    template<typename TOutput>
    auto TryGet() const -> const TOutput*
    {
        const auto key = std::type_index(typeid(TOutput));
        auto it = m_outputs.find(key);
        if (it == m_outputs.end()) return nullptr;
        return static_cast<const TOutput*>(it->second.get());
    }

private:
    friend class AppBuilder; // only AppBuilder can populate

    struct OutputDeleter
    {
        void (*Destroy)(void*) = nullptr;
        void operator()(void* p) const { if (Destroy) Destroy(p); }
    };

    std::unordered_map<std::type_index,
        std::unique_ptr<void, OutputDeleter>> m_outputs;
};
```

The type-erased storage uses `unique_ptr<void, OutputDeleter>` with a custom deleter that knows the concrete type. This avoids `std::any` overhead and aligns with the existing codebase pattern of type_index-keyed maps.

An alternative: derive all output types from a common base. This adds an unwanted inheritance requirement. The void-pointer-with-deleter approach is the standard C++ type-erasure pattern for heterogeneous ownership -- simpler and more flexible.

**Confidence:** HIGH - Standard type-erasure technique, no library dependencies.

### Pattern 5: Builder Lambda Lifecycle Hooks

**What:** Plugins register lifecycle callbacks via AppBuilder methods: `OnAppReady(lambda)`, `OnStateEnter<T>(lambda)`, etc. (D-04). Hooks are stored in a LifecycleHookRegistrar, whose Finalise() produces a LifecycleHookManifest consumed by Application.

**Design:**

```cpp
// In AppBuilder - convenience wrappers
void OnAppReady(std::function<void(EngineContext&)> callback);
void OnShutdown(std::function<void()> callback);

template<typename TState>
void OnStateEnter(std::function<void(EngineContext&)> callback);

template<typename TState>
void OnStateExit(std::function<void(EngineContext&)> callback);
```

**Storage:**

```cpp
struct LifecycleHookManifest
{
    std::vector<std::function<void(EngineContext&)>> OnAppReady;
    std::vector<std::function<void()>> OnShutdown;
    std::unordered_map<std::type_index,
        std::vector<std::function<void(EngineContext&)>>> OnStateEnter;
    std::unordered_map<std::type_index,
        std::vector<std::function<void(EngineContext&)>>> OnStateExit;
};
```

The state-typed hooks use `std::type_index` as the key, matching the established pattern. Application fires hooks at the correct lifecycle points by iterating the vectors.

**Confidence:** HIGH - Standard callback collection pattern.

### Pattern 6: SubsystemManifest Retrofit

**What:** SubsystemRegistry::Finalise() currently returns `Result<void>` and mutates internal state. The retrofit changes it to return `Result<SubsystemManifest<TBase>>` where SubsystemManifest is a read-only view of the sorted subsystems with Get<T>/TryGet<T> accessors (D-15).

**Approach:** SubsystemManifest takes ownership of the initialised subsystem instances and the topological order. SubsystemRegistry becomes a build-time-only object that is discarded after Finalise() returns the manifest. The existing tests in SubsystemTests.cpp provide a safety net for this refactor.

**Key consideration:** The current SubsystemRegistry owns both the registration state AND the live instances, and provides Initialise() + Shutdown(). Post-retrofit, the split is:
- SubsystemRegistry: registration + Finalise() -> returns manifest (build phase)
- SubsystemManifest: owns instances, Initialise() + Shutdown() + Get/TryGet (runtime phase)

This is a mechanical refactor of ~100 LOC as noted in CONTEXT.md.

**Confidence:** HIGH - Existing tests cover the important behaviour.

### Pattern 7: ConfigService with 3-Tier TOML Layering

**What:** ConfigService is an AppSubsystem providing address-stable config storage. Build phase: plugins declare config types via ConfigRegistrar. Runtime: ConfigService loads TOML files, applies layering, stores results.

**TOML merging with tomlplusplus:**

tomlplusplus does NOT have a built-in table merge function. The merging must be implemented manually. The approach:

```cpp
/// Merge source table into destination. Source keys override destination.
/// Only top-level keys are merged (shallow merge) -- sufficient for
/// per-section config files where each section is a flat struct.
void MergeTable(toml::table& destination, const toml::table& source)
{
    for (auto&& [key, value] : source)
    {
        destination.insert_or_assign(key, value);
    }
}
```

**3-tier loading flow:**

```cpp
auto LoadConfig(std::string_view key, const std::filesystem::path& configDir,
                const std::filesystem::path& savedDir) -> toml::table
{
    toml::table merged;

    // Layer 2: project config
    const auto projectPath = configDir / std::format("{}.toml", key);
    if (std::filesystem::exists(projectPath))
    {
        merged = toml::parse_file(projectPath.string());
        Log::Info(LogConfig, "Loaded {}", projectPath.string());
    }
    else
    {
        Log::Info(LogConfig, "No {} found, using defaults", projectPath.string());
    }

    // Layer 3: user overrides
    const auto savedPath = savedDir / "config" / std::format("{}.toml", key);
    if (std::filesystem::exists(savedPath))
    {
        auto overrides = toml::parse_file(savedPath.string());
        MergeTable(merged, overrides);
        Log::Info(LogConfig, "Applied user overrides from {}", savedPath.string());
    }

    return merged;
}
```

Layer 1 (struct defaults) is the initial state of T{} before TOML values are applied.

**File caching (D-07):**

```cpp
std::unordered_map<std::string, toml::table> m_tableCache;
```

When multiple plugins call `LoadConfig<T>("physics")`, the TOML file is only parsed once. The cache is keyed by the config file key string.

**Address-stable storage:**

ConfigService stores config structs in heap-allocated storage. Pointers to these structs remain valid for the application's lifetime. This enables future cvar binding (console system scope, not Phase 3) where cvars hold pointers to config fields.

```cpp
std::unordered_map<std::type_index,
    std::unique_ptr<void, void(*)(void*)>> m_configs;
```

**Deserialisation pattern:**

The planner needs to decide on a deserialisation approach. Two viable options:

**Option A: Manual deserialisation functions (recommended for Phase 3).**
Each config struct provides a static factory:
```cpp
struct PhysicsConfig
{
    float FixedTimestep = 1.0f / 60.0f;
    // ...

    static auto FromToml(const toml::table& tbl) -> PhysicsConfig;
};
```
This matches the existing EngineConfig.cpp pattern. No reflection needed.

**Option B: Reflection-based deserialisation (future).**
A compile-time or macro-based reflection system that auto-generates TOML->struct mappings. More DRY but requires building the reflection machinery. Better suited for a later phase when the config system matures.

**Recommendation:** Option A for Phase 3. It's proven in the codebase, zero new dependencies, and aligns with the principle of minimal implementation now.

**Confidence:** HIGH for the layering and caching. The tomlplusplus API is well-understood from existing usage in EngineConfig.cpp and ProjectDescriptor.cpp.

### Anti-Patterns to Avoid

- **Frozen-registrar pattern (leaking builder state):** Don't keep registrar objects alive after Finalise. They are build-time scaffolding. Processed outputs replace them entirely (D-12).

- **Virtual dispatch for concept-dispatched types:** Plugin groups should NOT inherit from IPlugin. Concept dispatch at compile time is strictly better -- no vtable overhead, clearer type safety, and groups are transparent (D-03).

- **Global/singleton config access:** ConfigService is an AppSubsystem, accessed through EngineContext like any other subsystem. No static accessors, no global config object. This maintains "engine is a library" -- multiple Application instances could have separate config.

- **Recursive table merge for config:** The override files (saved/config/) should use shallow merge at the section level. Deep recursive merge of nested TOML tables creates hard-to-debug layering conflicts. Each config struct is relatively flat.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Topological sort | New implementation in AppBuilder | Extract shared utility from SubsystemRegistry | Already proven with cycle detection, error reporting |
| TOML parsing | Custom parser or string-based loader | tomlplusplus (already linked) | Handles TOML spec edge cases, error reporting with line numbers |
| Type-erased map | Custom `std::any` containers | `std::type_index` + `unique_ptr<void, Deleter>` | Matches SubsystemRegistry/ServiceProvider pattern, no runtime overhead |
| Error accumulation | Ad hoc bool + string pairs | Dedicated ValidationResult type | Compiler-style error reporting needs ordered, categorised errors |
| Config file path resolution | String concatenation | `std::filesystem::path` | Already used in EngineConfig.cpp, ProjectDescriptor.cpp |

## Common Pitfalls

### Pitfall 1: Registrar Finalise Order Matters

**What goes wrong:** Cross-registrar validation needs outputs from other registrars, but they haven't finalised yet.
**Why it happens:** Registrars are stored in an unordered map -- iteration order is undefined.
**How to avoid:** Finalise all registrars independently first (collecting Results), THEN run cross-registrar validation as a second pass over the collected outputs. This aligns with D-13 (smart-accumulation).
**Warning signs:** "Output not found" errors during Finalise -- usually means validation is querying a registrar that hasn't been finalised yet.

### Pitfall 2: Plugin Describe() Called Too Early

**What goes wrong:** `Describe()` is called before the plugin has access to any runtime state (it's a const method on an interface).
**Why it happens:** Describe() must return static metadata. If someone tries to make dependencies dynamic, they break the build-phase ordering.
**How to avoid:** PluginDescriptor fields must be compile-time constant or constructible from type information only. Use `std::type_index(typeid(OtherPlugin))` for DependsOn entries.
**Warning signs:** Describe() implementations that try to read config or query state.

### Pitfall 3: Config Struct Lifetime vs Pointer Stability

**What goes wrong:** ConfigService returns `const T&` but the underlying storage is moved or reallocated, invalidating references.
**Why it happens:** If configs are stored in a vector or moved during reallocation.
**How to avoid:** Use heap-allocated individual config structs (unique_ptr per config type). Never move or reallocate after initial storage. Address stability is the explicit design requirement (D-10, console.md backing).
**Warning signs:** Use-after-free in subsystems holding config references.

### Pitfall 4: Duplicate Plugin Detection

**What goes wrong:** The same plugin type is added twice (directly and via a group), causing duplicate registrations.
**Why it happens:** Groups transparently expand -- the caller may not realise a group includes a plugin they've also added explicitly.
**How to avoid:** Track registered plugin types by `std::type_index`. Skip duplicates with a info-level log (not an error -- Bevy does the same). First registration wins.
**Warning signs:** Duplicate subsystem/state registrations flowing from double Build() calls.

### Pitfall 5: AppBuilder Used After Finalise

**What goes wrong:** Plugin code holds a reference to AppBuilder and tries to register after Finalise().
**Why it happens:** Builder reference escaping into long-lived lambdas or subsystems.
**How to avoid:** Assert on registration after Finalise (SubsystemRegistry already does this). AppBuilder should track its state (building vs finalised). Consider making AppBuilder move-only and consuming it during Finalise: `auto Finalise(AppBuilder builder) -> Result<AppDescriptor>`.
**Warning signs:** Assertions about "cannot register after Finalise".

### Pitfall 6: TOML Parse Errors During Config Loading

**What goes wrong:** Malformed TOML in config files causes a `toml::parse_error` exception, crashing before the engine starts.
**Why it happens:** User-edited config files can have syntax errors. tomlplusplus throws on parse errors by default.
**How to avoid:** Wrap `toml::parse_file` in try/catch (already done in EngineConfig.cpp). Return Result<T> with the error message. Fallback to struct defaults on parse failure.
**Warning signs:** Unhandled exceptions from config loading in headless/test environments.

## Code Examples

### Type-Erased Registrar Store - Retrieval and Convenience Wrappers

```cpp
// Source: Derived from SubsystemRegistry.h pattern and console.md

// AppBuilder provides typed access to registrars
template<typename TRegistrar>
    requires std::derived_from<TRegistrar, IRegistrar>
auto AppBuilder::Registrar() -> TRegistrar&
{
    const auto key = std::type_index(typeid(TRegistrar));
    auto it = m_registrars.find(key);
    if (it == m_registrars.end())
    {
        auto [inserted, ok] = m_registrars.emplace(
            key, std::make_unique<TRegistrar>());
        WAYFINDER_ASSERT(ok, "Failed to create registrar: {}",
            typeid(TRegistrar).name());
        it = inserted;
    }
    return static_cast<TRegistrar&>(*it->second);
}

// Convenience wrappers delegate to typed registrars
void AppBuilder::AddState(StateDescriptor descriptor)
{
    Registrar<StateRegistrar>().Register(std::move(descriptor));
}

template<typename T>
    requires std::derived_from<T, AppSubsystem>
void AppBuilder::RegisterSubsystem(SubsystemDescriptor descriptor)
{
    Registrar<SubsystemRegistrar<AppSubsystem>>().Register<T>(
        std::move(descriptor));
}
```

### Plugin with Dependencies

```cpp
// Source: Pattern derived from D-01 and architecture doc

class PhysicsPlugin : public IPlugin
{
public:
    auto Describe() const -> PluginDescriptor override
    {
        return {
            .Name = "Physics"_is,
            .DependsOn = { std::type_index(typeid(CorePlugin)) },
        };
    }

    void Build(AppBuilder& builder) override
    {
        auto config = builder.LoadConfig<PhysicsConfig>("physics");
        builder.RegisterSubsystem<PhysicsWorld>({
            .RequiredCapabilities = Caps(Capability::Physics),
            .DependsOn = Deps<TimeSubsystem>(),
        });
    }
};
```

### Concept-Based Plugin Group

```cpp
// Source: Pattern from D-03 and Bevy PluginGroup analogy

struct DefaultPlugins
{
    void Build(AppBuilder& builder) const
    {
        builder.AddPlugin<CorePlugin>();
        builder.AddPlugin<WindowPlugin>();
        builder.AddPlugin<RenderingPlugin>();
        builder.AddPlugin<PhysicsPlugin>();
    }
};

static_assert(PluginGroupType<DefaultPlugins>);
static_assert(not PluginType<DefaultPlugins>);
```

### Smart-Accumulation Validation

```cpp
// Source: Pattern from D-13

struct ValidationError
{
    std::string Registrar;
    std::string Message;
};

class ValidationResult
{
public:
    void AddError(std::string_view registrar, std::string message)
    {
        m_errors.push_back({std::string(registrar), std::move(message)});
    }

    [[nodiscard]] auto HasErrors() const -> bool
    {
        return not m_errors.empty();
    }

    [[nodiscard]] auto Errors() const
        -> std::span<const ValidationError>
    {
        return m_errors;
    }

    [[nodiscard]] auto ToError() const -> Error
    {
        std::string combined;
        for (const auto& err : m_errors)
        {
            combined += std::format("[{}] {}\n", err.Registrar, err.Message);
        }
        return Error(std::move(combined));
    }

private:
    std::vector<ValidationError> m_errors;
};
```

### TOML Config Loading

```cpp
// Source: Derived from EngineConfig.cpp pattern + D-06 through D-09

struct PhysicsConfig
{
    float FixedTimestep = 1.0f / 60.0f;
    float Gravity = -9.81f;
    uint32_t VelocityIterations = 8;
    uint32_t PositionIterations = 3;

    static auto FromToml(const toml::table& tbl) -> PhysicsConfig
    {
        PhysicsConfig config{};  // Layer 1: struct defaults
        if (auto v = tbl["fixed_timestep"].value<double>())
            config.FixedTimestep = static_cast<float>(*v);
        if (auto v = tbl["gravity"].value<double>())
            config.Gravity = static_cast<float>(*v);
        if (auto v = tbl["velocity_iterations"].value<uint32_t>())
            config.VelocityIterations = *v;
        if (auto v = tbl["position_iterations"].value<uint32_t>())
            config.PositionIterations = *v;
        return config;
    }
};
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| PluginRegistry (v1.5) | AppBuilder + typed registrar store | Phase 3 | All plugin registration flows through builder |
| Monolithic EngineConfig | Per-plugin config structs + ConfigService | Phase 3 | Each plugin owns its config shape |
| Frozen registrar (mutable after finalise) | Processed output (immutable after finalise) | Phase 3 | Thread-safe, clear build/runtime separation |
| SubsystemRegistry owns runtime + build state | SubsystemManifest (runtime) + SubsystemRegistry (build) | Phase 3 | Clean separation of concerns |

## Reference Engine Analysis

### Bevy (Rust, 2024) - HIGH confidence

Bevy's plugin system is the closest architectural match to Wayfinder's target design:

- **Plugin trait:** `build(&self, app: &mut App)` -- maps to Wayfinder's `Build(AppBuilder&)`.
- **PluginGroup trait:** Separate from Plugin. `build(self) -> PluginGroupBuilder` -- maps to D-03's concept-based groups.
- **Duplicate detection:** `App::add_boxed_plugin` checks if a plugin with the same name is already registered. Logs a warning and skips. Maps to Pitfall 4 recommendation.
- **PluginGroupBuilder ordering:** Supports `add_before<T>()`, `add_after<T>()`, `enable<T>()`, `disable<T>()` -- more complex than Wayfinder needs in Phase 3, but shows evolutionary direction.
- **Tuple-based variadic adding:** `add_plugins((A, B, C))` via the `Plugins<Marker>` trait for up to 15 elements. Wayfinder achieves this via the plugin group concept.
- **TypeId mapping:** Uses `TypeId`-keyed `HashMap` for plugin state -- direct equivalent of `std::type_index`-keyed `std::unordered_map`.
- **Lifecycle:** `build()`, `ready()`, `finish()`, `cleanup()` -- four hook points vs Wayfinder's OnAppReady/OnStateEnter/OnStateExit/OnShutdown. Wayfinder's hooks are richer (typed state transitions).

**Key takeaway:** Bevy validates the Plugin/PluginGroup separation, type-id-keyed storage, and duplicate detection. Wayfinder's concept-based approach is a clean C++23 translation.

### O3DE (C++17, 2024) - MEDIUM confidence

O3DE (Open 3D Engine) uses a heavyweight Module/Gem system:

- **AZ::Module:** Base class for engine modules (Gems). Each declares `GetRequiredSystemComponents()` -- component descriptors, not plugin dependencies.
- **ComponentApplication:** Loads modules (static + dynamic), creates SystemEntity, activates components. Complex lifecycle with `ModuleInitializationSteps` enum.
- **SettingsRegistry:** Centralised config using JSON-based .setreg files. Very different from Wayfinder's TOML-per-plugin approach.
- **RTTI + EBus:** Heavy use of RTTI macros and event buses for cross-module communication. Wayfinder explicitly avoids this (composition over inheritance, explicit over implicit).

**Key takeaway:** O3DE's Module pattern validates the "module declares what components it provides" concept, but its enterprise-style complexity (RTTI macros, EBus) is the opposite of Wayfinder's goals. The `GetRequiredSystemComponents()` return type list is analogous to PluginDescriptor.DependsOn in spirit.

### Spartan Engine (C++20, 2024) - LOW confidence

Spartan is monolithic -- no plugin/module system. Editor widgets are registered by direct construction in `Editor::Editor()`. Components use a macro-based registration (`SP_COMPONENT_LIST`). Not useful as a plugin architecture reference.

## Open Questions

1. **SubsystemRegistrar naming: SubsystemRegistrar<AppSubsystem> vs AppSubsystemRegistrar**
   - What we know: The architecture doc shows `builder.Registrar<SubsystemRegistrar<AppSubsystem>>()`.
   - What's unclear: Whether the template parameter or a typedef provides better ergonomics.
   - Recommendation: Use the template form for flexibility (works with both scopes), provide a convenience typedef `using AppSubsystemRegistrar = SubsystemRegistrar<AppSubsystem>` if it improves readability.

2. **ConfigService initialisation timing**
   - What we know: ConfigService is an AppSubsystem. It needs to be available during Build() (plugins call LoadConfig) but also exists as a regular subsystem.
   - What's unclear: How to square "available during Build phase" with "initialised during Initialise phase."
   - Recommendation: AppBuilder holds a pre-ConfigService config cache during Build(). The actual ConfigService subsystem is populated from this cache during its Initialise(). The builder.LoadConfig<T>() method loads from disk into the cache. ConfigService::Initialise() takes ownership of the cached configs and becomes the runtime accessor. This preserves the AppSubsystem lifecycle contract.

3. **PluginDescriptor.Name: InternedString vs std::string**
   - What we know: InternedString is preferred for "stable, repeatedly compared identifiers" per copilot-instructions.md.
   - What's unclear: Plugin names are compared during duplicate detection and dependency resolution. Are they compared frequently enough to warrant interning?
   - Recommendation: Use InternedString. Plugin names are compared during Build phase (duplicate detection) and appear in error messages. The interning cost at registration time is negligible, and it aligns with how the codebase handles similar identifiers.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | doctest (via `doctest::doctest_with_main`) |
| Config file | Linked via CMake, no config file needed |
| Quick run command | `cmake --build --preset debug --target wayfinder_core_tests && ctest --preset test -R core` |
| Full suite command | `ctest --preset test` |

### Phase Requirements -> Test Map
| Req ID | Behaviour | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| PLUG-02 | AppBuilder typed registrar store creation and retrieval | unit | `ctest --preset test -R core` | No - Wave 0 |
| PLUG-03 | Plugin dependency ordering, cycle detection, missing deps | unit | `ctest --preset test -R core` | No - Wave 0 |
| PLUG-04 | Plugin group expansion, concept dispatch, transparent composition | unit | `ctest --preset test -R core` | No - Wave 0 |
| PLUG-05 | AppDescriptor immutability, output retrieval, Finalise round-trip | unit | `ctest --preset test -R core` | No - Wave 0 |
| PLUG-06 | Lifecycle hook storage, ordering, firing | unit | `ctest --preset test -R core` | No - Wave 0 |
| CFG-01 | Config 3-tier layering, missing file defaults, parse error handling | unit | `ctest --preset test -R core` | No - Wave 0 |
| CFG-02 | LoadConfig caching, same-file multi-read | unit | `ctest --preset test -R core` | No - Wave 0 |
| APP-01 | Application::AddPlugin<T>(), group expansion, end-to-end | integration | `ctest --preset test -R core` | No - Wave 0 |

### Sampling Rate
- **Per task commit:** `cmake --build --preset debug --target wayfinder_core_tests && ctest --preset test -R core`
- **Per wave merge:** `ctest --preset test`
- **Phase gate:** Full suite green before `/gsd-verify-work`

### Wave 0 Gaps
- [ ] `tests/plugins/AppBuilderTests.cpp` -- covers PLUG-02, PLUG-03, PLUG-04, PLUG-06
- [ ] `tests/plugins/AppDescriptorTests.cpp` -- covers PLUG-05
- [ ] `tests/plugins/ConfigServiceTests.cpp` -- covers CFG-01, CFG-02
- [ ] `tests/app/SubsystemTests.cpp` -- UPDATE existing file for SubsystemManifest retrofit
- [ ] `tests/fixtures/config/` -- test TOML files for config loading tests
- [ ] Update `tests/CMakeLists.txt` -- add new test source files to wayfinder_core_tests

## Sources

### Primary (HIGH confidence)
- Wayfinder codebase: SubsystemRegistry.h, ServiceProvider.h, EngineConfig.cpp, ProjectDescriptor.cpp, IPlugin.h, all registrars, EngineContext.h, Application.h
- `docs/plans/application_architecture_v2.md` -- AppBuilder, AppDescriptor, plugin config, lifecycle
- `docs/plans/console.md` -- ConfigService, ConfigRegistrar, cvar backing, OnConfigReloaded
- `.planning/phases/03-plugin-composition/03-CONTEXT.md` -- all locked decisions D-01 through D-17
- `.planning/REQUIREMENTS.md` -- PLUG-02 through PLUG-06, CFG-01, CFG-02, APP-01
- bevyengine/bevy GitHub -- Plugin trait, PluginGroup trait, PluginGroupBuilder, App::add_plugins

### Secondary (MEDIUM confidence)
- o3de/o3de GitHub -- AZ::Module, ComponentApplication, ModuleManager, SettingsRegistry
- tomlplusplus documentation -- table access API, parse_file, value extraction

### Tertiary (LOW confidence)
- PanosK92/SpartanEngine GitHub -- confirmed no plugin system (monolithic architecture)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - all libraries already in the project (tomlplusplus, doctest, std library)
- Architecture: HIGH - patterns directly derived from existing codebase + reference engines
- Pitfalls: HIGH - derived from actual codebase patterns and real type-erasure experience
- Config system: HIGH - tomlplusplus API verified from existing EngineConfig.cpp usage

**Research date:** 2025-07-15
**Valid until:** 2025-08-15 (stable domain - C++23 patterns, no rapidly evolving dependencies)
