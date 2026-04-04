# Phase 3: Plugin Composition - Discussion Log

**Session:** 2026-04-04 to 2026-04-05
**Gray areas discussed:** 4 of 4 selected

---

## Gray Area 1: Plugin Dependency & Ordering

### Question
How do plugins declare dependencies and how are Build() calls ordered?

### Approaches Considered
1. **Descriptor-based (SubsystemDescriptor pattern):** `PluginDescriptor{.DependsOn}` passed to registration
2. **Type trait / concept:** `static constexpr auto DEPENDS_ON = std::tuple<PhysicsPlugin, ...>`
3. **In-Build declaration (Bevy-style):** Declared inside Build() via builder calls

### Engine Research
- **Bevy:** IPlugin trait has `build(app)` and optional `dependencies() -> Vec<PluginId>`. Groups via PluginGroup trait. Build() ordering is NOT dependency-based -- Bevy doesn't order Build() calls. Ordering happens at system/schedule level.
- **Oxylus:** Simple App with add_layer/add_system. No plugin dependency system.
- **O3DE:** Gem system with module descriptors containing dependency lists. Build-time dependency resolution via CMake.

### Key Insight
Bevy's approach of not ordering Build() calls works because Bevy's Build() only adds things to the App -- it doesn't access other plugins' registrations. The same applies to our AppBuilder: Build() calls register, Finalise() resolves. So dependency ordering of Build() calls is for correctness guarantees and error reporting, not for data access ordering.

### Decision
Virtual `Describe()` on IPlugin returning `PluginDescriptor{.DependsOn}`. Chosen for cohesion with SubsystemDescriptor pattern. Default implementation returns empty descriptor. User confirmed deps should be internalised (no external modification).

### Also Decided: Plugin Groups
Concept-based groups (struct with `Build(AppBuilder&)` that is NOT IPlugin). Groups are transparent composition. Matches Bevy's PluginGroup spirit but lighter.

---

## Gray Area 2: Lifecycle Hooks

### Question
What lifecycle hooks do plugins register and via what mechanism?

### Approaches Considered
1. **Virtual methods on IPlugin:** OnReady(), OnShutdown(), etc.
2. **Builder lambdas:** builder.OnAppReady(lambda), builder.OnStateEnter<T>(lambda)
3. **Event bus subscription:** Subscribe to lifecycle events

### Analysis
- Virtual methods: clean but couples all plugins to all hook points (empty overrides everywhere), can't have typed state hooks
- Builder lambdas: opt-in, typed state hooks natural, IPlugin stays clean
- Event bus: most flexible but indirection, ordering unclear, harder to validate at Finalise time

### Small vs Large Game Scale Analysis
Discussed examples at 5-plugin and 20-plugin scale. Builder lambdas scaled best because:
- Only plugins that need hooks register them (no empty overrides)
- Typed state hooks (`OnStateEnter<GameplayState>`) avoid string-based dispatch
- Hook execution order follows plugin dependency order (natural from the dependency graph)

### Decision
Builder lambdas. Hook points: OnAppReady, OnStateEnter<T>, OnStateExit<T>, OnShutdown. No frame-level hooks (subsystems and render features cover per-frame work).

---

## Gray Area 3: Per-Plugin Configuration

### Question
How do plugins declare, load, and receive configuration?

### Context Recovery
User referenced `config_service.md` which was never created (server issues during original drafting). Investigated `console.md` which contains extensive ConfigService references: ConfigRegistrar, ConfigService as AppSubsystem, address-stable config storage, cvar binding, OnConfigReloaded(), saved/user_overrides.toml. Also reviewed `application_architecture_v2.md` Configuration section showing `builder.LoadConfig<T>("physics")` pattern.

### Layering Discussion
User wanted per-plugin files for moddability/packaging. Discussed override mechanisms:
- Single monolithic override file vs per-plugin overrides
- `saved/` directory organisation

### Engine Scale Clarification
User clarified Wayfinder targets: sixth-gen aesthetics with modern hardware (NOT low-poly indie), WoW++-scale scenes (hundreds to thousands of characters, dense environments), multi-platform (PC, console, mobile), real-time GI and lighting but stylised not photorealistic. This confirmed per-plugin config is the right granularity -- a monolithic config doesn't scale to this plugin count.

### Decision
- 3-tier layering: struct defaults -> config/<key>.toml -> saved/config/<key>.toml
- One file per config key, cached loading
- Per-plugin override files in saved/config/ (mirrors config/ structure)
- Log on load (Info level), missing files return defaults without error
- Full ConfigService in Phase 3 (AppSubsystem, address-stable storage, OnConfigReloaded stub)
- Platform-conditional config as future extension point

---

## Gray Area 4: AppDescriptor & Validation

### Question
What does AppDescriptor contain, how is it validated, and who owns it?

### Storage Model Discussion
Compared two approaches in detail with implementation examples:

**Frozen registrars:** Move registrars from AppBuilder to AppDescriptor, lock with Finalise guard. Matches existing SubsystemRegistry pattern. Issues: leaky interface (builder methods visible on const ref), runtime guard not compile-time, carries dead builder state.

**Processed outputs:** Each registrar Finalise() -> Result<OutputType>. AppDescriptor holds outputs only. Clean compile-time immutability, no leaky interfaces, thread-safe by construction. Cost: more types (one output type per registrar).

### Validation Strategy Discussion
Clarified three options:
- Fail-fast: stop on first error (maddening at scale)
- Per-registrar Result: one error per domain per pass
- Accumulate all: every independent error in one pass

User initially unclear on difference between fail-fast and per-registrar. Provided concrete examples with 15-plugin, 8-state, 40-subsystem scenario showing iteration counts.

### Decision
- Processed outputs (D-12). Each registrar produces a domain-specific read-only type.
- Smart-accumulation validation (D-13). Compiler-style: accumulate within registrar, continue across, cross-checks gated on all-pass.
- Application owns AppDescriptor, EngineContext references (D-14). Consistent with Phase 2.
- Retrofit SubsystemRegistry (D-15) to processed-output pattern to avoid two patterns at the AppBuilder integration point. Mechanical change, tests exist.

---

*Discussion complete. All 4 gray areas resolved with locked decisions.*
