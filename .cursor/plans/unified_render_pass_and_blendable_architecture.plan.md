# Plan: Unified Render Pass Pipeline & Data-Driven Post-Process Architecture

## TL;DR

Replace the early/game/late three-band pass system with a single unified phase-ordered pass list. Introduce a `PostProcessColour` resource convention so post-process passes chain without knowing about each other. Add TOML schema files per effect type for editor UI generation and validation. Lay the foundation for fully data-driven pipeline configuration (TOML files defining which passes are active and in what order). This is three phases: **Phase A** (immediate, code) is the unified pipeline + post-process colour convention. **Phase B** (near-term, data+code) is effect schemas. **Phase C** (future, data-driven) is pipeline configuration from files.

---

## Phase A: Unified Pass Pipeline + PostProcessColour Convention

### A.1 — Expand `EngineRenderPhase`

**File:** [engine/wayfinder/src/rendering/pipeline/RenderPipeline.h](engine/wayfinder/src/rendering/pipeline/RenderPipeline.h)

Replace the current 6-value enum with a more expressive set of injection bands:

```
enum class RenderPhase : uint8_t
{
    PreOpaque       = 0,   // Shadow maps, GBuffer prep, pre-pass work
    Opaque          = 1,   // Forward/deferred main scene geometry
    PostOpaque      = 2,   // SSR, SSAO, light compositing — reads scene colour/depth/GBuffer
    Transparent     = 3,   // Transparent geometry (alpha-blended)
    PostProcess     = 4,   // Bloom, DOF, motion blur — reads/writes PostProcessColour
    Composite       = 5,   // Tonemapping, colour grading, FXAA — final image assembly
    Overlay         = 6,   // Debug draws, editor gizmos, UI — on top of everything
    Present         = 7,   // Swapchain blit — exactly one pass should live here
};
```

**Key changes from current:**
- Renamed from `EngineRenderPhase` to `RenderPhase` — it's no longer engine-only.
- `Debug` → `Overlay` (clearer intent, can hold editor gizmos too).
- Added `Transparent`, `PostProcess`, `Composite`, `Overlay`, `Present` as distinct bands.
- Removed `PreComposite` (subsumed by `PostProcess`) and `LateEngine` (subsumed by `Composite`/`Present`).

### A.2 — Unify pass storage in `RenderPipeline`

**Files:** [engine/wayfinder/src/rendering/pipeline/RenderPipeline.h](engine/wayfinder/src/rendering/pipeline/RenderPipeline.h), [engine/wayfinder/src/rendering/pipeline/RenderPipeline.cpp](engine/wayfinder/src/rendering/pipeline/RenderPipeline.cpp)

Replace the two vectors (`m_earlyEnginePasses`, `m_lateEnginePasses`) with a single sorted vector:

```
struct PassSlot
{
    RenderPhase Phase = RenderPhase::Opaque;
    int32_t Order = 0;           // Lower runs first within the same phase
    uint32_t InsertSequence = 0; // Tiebreaker for same phase+order
    std::unique_ptr<RenderPass> Pass;
};

std::vector<PassSlot> m_passes;  // Single list, sorted by (Phase, Order, InsertSequence)
```

`RegisterPass(RenderPhase phase, int32_t order, std::unique_ptr<RenderPass> pass)` replaces both `RegisterEnginePass` and the old `AddPass`. The sort key is `(Phase, Order, InsertSequence)`.

`BuildGraph` becomes:
```
void BuildGraph(RenderGraph& graph, const RenderPipelineFrameParams& params) const
{
    for (const auto& slot : m_passes)
    {
        if (slot.Pass && slot.Pass->IsEnabled())
        {
            slot.Pass->AddPasses(graph, params);
        }
    }
}
```

No early/game/late split. No separate `gamePasses` span argument.

### A.3 — Update `Renderer` public API

**Files:** [engine/wayfinder/src/rendering/pipeline/Renderer.h](engine/wayfinder/src/rendering/pipeline/Renderer.h), [engine/wayfinder/src/rendering/pipeline/Renderer.cpp](engine/wayfinder/src/rendering/pipeline/Renderer.cpp)

Remove: `AddPass(unique_ptr<RenderPass>)`, `RegisterEnginePass(EngineRenderPhase, int32_t, unique_ptr<RenderPass>)`, `m_passes` member.

New API:
```
/// Register a pass at a specific phase and order. All passes (engine and game) use this.
void AddPass(RenderPhase phase, int32_t order, std::unique_ptr<RenderPass> pass);

/// Convenience: register a pass at a phase with default order 0.
void AddPass(RenderPhase phase, std::unique_ptr<RenderPass> pass);
```

`RemovePass<T>()` and `GetPass<T>()` remain but now search the pipeline's unified list instead of `m_passes`.

The `Renderer::Render()` call to `BuildGraph` drops the `m_passes` argument — the pipeline owns everything.

### A.4 — Update engine pass registration

**File:** [engine/wayfinder/src/rendering/pipeline/RenderPipeline.cpp](engine/wayfinder/src/rendering/pipeline/RenderPipeline.cpp) `Initialise()`

```cpp
RegisterPass(RenderPhase::Opaque,    0, std::make_unique<SceneOpaquePass>());
RegisterPass(RenderPhase::Overlay,   0, std::make_unique<DebugPass>());
RegisterPass(RenderPhase::Present,   0, std::make_unique<CompositionPass>());
```

`CompositionPass` moves from `LateEngine` to `Present` — it's the swapchain writer. When a dedicated present-source copy pass is needed, it goes at `Composite` phase.

### A.5 — Introduce `PostProcessColour` resource convention

**Implementation status:** Not yet landed. The codebase still uses `GraphTextureId::PresentSource` / `GraphTextures::PresentSource` and `CompositionPass` resolves `PresentSource` or falls back to `SceneColour` (see `CompositionPass.cpp`, `RenderGraph.h`). The snippets below describe the planned API; landing them will require updating those call sites and any `PresentSource` consumers.

**File:** [engine/wayfinder/src/rendering/graph/RenderGraph.h](engine/wayfinder/src/rendering/graph/RenderGraph.h)

Add to `GraphTextureId` and `GraphTextures`:

```cpp
enum class GraphTextureId : uint8_t
{
    SceneColour,
    SceneDepth,
    PresentSource,
    PostProcessColour,  // NEW: output of the latest post-process pass
};

namespace GraphTextures
{
    // ...existing...
    inline const InternedString PostProcessColour = InternedString::Intern("PostProcessColour");
}
```

**Convention (documented, not enforced by code until A.5–A.7 are landed):**
- Post-process passes in the `PostProcess` phase read `PostProcessColour` (falling back to `SceneColour` if no prior post-process pass wrote it) and write a new transient named `PostProcessColour`.
- `CompositionPass` reads `PostProcessColour` (falling back to `SceneColour`).
- Each write to `PostProcessColour` creates a new graph handle; the graph tracks the latest writer, so `FindHandle("PostProcessColour")` always resolves to the most recent version.
- Private intermediates (e.g. BloomHalfRes, BloomQuarterRes) use arbitrary names and are invisible to other passes.

**File:** [engine/wayfinder/src/rendering/graph/RenderGraph.h](engine/wayfinder/src/rendering/graph/RenderGraph.h) or new header `PostProcessColourUtils.h`

Add a convenience function:

```cpp
/// Resolves the current post-process colour input: `PostProcessColour` if any prior 
/// post-process pass wrote it, otherwise `SceneColour`. Returns invalid handle only 
/// if neither exists (error — scene pass didn't run).
RenderGraphHandle ResolvePostProcessInput(const RenderGraph& graph);
```

Implementation:
```cpp
RenderGraphHandle ResolvePostProcessInput(const RenderGraph& graph)
{
    auto colour = graph.FindHandle(GraphTextureId::PostProcessColour);
    if (colour.IsValid()) return colour;
    return graph.FindHandleChecked(GraphTextureId::SceneColour);
}
```

### A.6 — Update `CompositionPass` to use `ResolvePostProcessInput`

**Implementation status:** Not yet landed (same as A.5).

**File:** [engine/wayfinder/src/rendering/passes/CompositionPass.cpp](engine/wayfinder/src/rendering/passes/CompositionPass.cpp)

Replace the current `SceneColour`/`PresentSource` resolution with:

```cpp
auto source = ResolvePostProcessInput(graph);
```

This means `CompositionPass` reads whatever the last post-process pass produced (or raw `SceneColour` if none ran). No hardcoded list of effect output names.

### A.7 — Update `PresentSourceCopyPass`

**File:** [engine/wayfinder/src/rendering/passes/PresentSourceCopyPass.cpp](engine/wayfinder/src/rendering/passes/PresentSourceCopyPass.cpp)

If still needed, register at `RenderPhase::Composite` and have it read `PostProcessColour` / `SceneColour` and write `PresentSource`. In general, this pass may become unnecessary if `CompositionPass` directly reads the post-process colour output.

### A.8 — Update `docs/render_passes.md`

Document:
- The new `RenderPhase` enum and what each phase is for.
- The `PostProcessColour` convention (read `PostProcessColour` or `SceneColour`, write `PostProcessColour`).
- The `ResolvePostProcessInput` helper.
- Migration from old `AddPass(pass)` / `RegisterEnginePass(phase, order, pass)` to unified `AddPass(phase, order, pass)`.
- Example: how a game developer writes a bloom pass, registers it at `PostProcess` order 0, and it automatically chains.

### A.9 — Tests

**File:** [tests/rendering/RenderPassTests.cpp](tests/rendering/RenderPassTests.cpp) (extend)
**File:** [tests/rendering/RenderPipelineTests.cpp](tests/rendering/RenderPipelineTests.cpp) (extend)

New test cases:
- **Phase ordering:** Register passes at PostProcess:100, Opaque:0, Present:0, PostProcess:0 — verify `BuildGraph` invokes `AddPasses` in (Opaque:0, PostProcess:0, PostProcess:100, Present:0) order.
- **PostProcessColour convention:** Two passes both read/write `PostProcessColour` — verify the graph compiles and the second pass reads the first pass's output (topological order enforced by `ReadTexture`/`WriteColour` on the same named resource).
- **`ResolvePostProcessInput` returns `SceneColour` when no `PostProcessColour` was written.**
- **`ResolvePostProcessInput` returns PostProcessColour when a prior pass wrote it.**
- **CompositionPass reads post-process colour output correctly** (existing test updated to use new API).
- **Remove pass from unified list — verify phase ordering preserved.**

### A.10 — CMakeLists.txt

Add any new headers (e.g. `PostProcessColourUtils.h` if created as a separate header).

---

## Phase B: Effect Schema System (TOML Metadata)

### B.1 — Schema file format

**New directory:** `engine/wayfinder/schemas/effects/`

One TOML file per registered effect type. Example:

```toml
# schemas/effects/colour_grading.toml
[effect]
name = "colour_grading"
display_name = "Colour Grading"
category = "colour"

[[fields]]
name = "exposure_stops"
type = "float"
default = 0.0
min = -10.0
max = 10.0
display = "Exposure (Stops)"

[[fields]]
name = "contrast"
type = "float"
default = 1.0
min = 0.0
max = 3.0
display = "Contrast"

[[fields]]
name = "saturation"
type = "float"
default = 1.0
min = 0.0
max = 3.0
display = "Saturation"

[[fields]]
name = "lift"
type = "float3"
default = [0.0, 0.0, 0.0]
min = [-1.0, -1.0, -1.0]
max = [1.0, 1.0, 1.0]
display = "Lift"

[[fields]]
name = "gamma"
type = "float3"
default = [1.0, 1.0, 1.0]
min = [0.01, 0.01, 0.01]
max = [3.0, 3.0, 3.0]
display = "Gamma"

[[fields]]
name = "gain"
type = "float3"
default = [1.0, 1.0, 1.0]
min = [0.0, 0.0, 0.0]
max = [5.0, 5.0, 5.0]
display = "Gain"
```

### B.2 — Schema loader

**New files:** `engine/wayfinder/src/rendering/materials/EffectSchema.h`, `engine/wayfinder/src/rendering/materials/EffectSchema.cpp`

Types:
```
enum class EffectFieldType { Float, Int, Float3, Colour, Bool };

struct EffectFieldSchema
{
    std::string Name;
    EffectFieldType Type;
    // Default/min/max stored as variant or typed union
    std::string DisplayName;
    std::string Tooltip;  // optional
};

struct EffectSchema
{
    std::string Name;           // matches registry name
    std::string DisplayName;
    std::string Category;
    std::vector<EffectFieldSchema> Fields;
};

/// Loads all .toml files from a directory, keyed by effect name.
Result<std::unordered_map<std::string, EffectSchema>> LoadEffectSchemas(const std::filesystem::path& schemaDir);
```

The schema loader is a standalone utility — it doesn't depend on the registry or any GPU code. Tools (Cartographer editor, validation tools) use it directly. The runtime engine may optionally load schemas for validation, but it's not required for rendering.

### B.3 — Schema-driven validation

**File:** [engine/wayfinder/src/scene/ComponentRegistry.cpp](engine/wayfinder/src/scene/ComponentRegistry.cpp) (or new validation module)

When loading a scene, if schemas are available, validate effect JSON against the schema:
- Unknown field names → warning
- Out-of-range values → clamp + warning
- Wrong type → error, skip field
- Missing required fields → use schema default

This supplements the existing `PostProcessRegistry` deserialisation but gives human-readable errors at load time before the C++ ADL `Deserialise` function runs.

### B.4 — Struct↔schema sync tests

**File:** `tests/rendering/EffectSchemaTests.cpp`

For each engine effect type, load its schema and verify field count and names match the C++ struct. This catches schema staleness:

```
TEST_CASE("Colour grading schema matches ColourGradingParams fields")
{
    auto schemas = LoadEffectSchemas("schemas/effects/");
    auto it = schemas.find("colour_grading");
    REQUIRE(it != schemas.end());
    const auto& schema = it->second;
    CHECK(schema.Fields.size() == 6); // exposure, contrast, saturation, lift, gamma, gain
    CHECK(schema.HasField("exposure_stops", EffectFieldType::Float));
    CHECK(schema.HasField("contrast", EffectFieldType::Float));
    CHECK(schema.HasField("saturation", EffectFieldType::Float));
    CHECK(schema.HasField("lift", EffectFieldType::Float3));
    CHECK(schema.HasField("gamma", EffectFieldType::Float3));
    CHECK(schema.HasField("gain", EffectFieldType::Float3));
}
```

### B.5 — CMakeLists.txt + docs

- Add schema files to an install/copy rule so they're available at runtime.
- Add new source files (EffectSchema.h/.cpp, test file).
- Document schema format in `docs/render_passes.md` or a new `docs/effect_schemas.md`.

---

## Phase C: Data-Driven Pipeline Configuration

### C.1 — Pipeline config file format

**New directory:** `config/pipelines/` (or `engine/wayfinder/config/pipelines/`)

```toml
# config/pipelines/default.toml
[pipeline]
name = "default"
description = "Default rendering pipeline with standard post-processing"

# Each entry maps a registered RenderPass (by GetName()) to a phase + order.
# Passes not listed here are not active in this pipeline configuration.
# Engine-critical passes (SceneOpaque, Composition) are always active.

[[passes]]
name = "Bloom"
phase = "post_process"
order = 0
enabled = true

[[passes]]
name = "DepthOfField"
phase = "post_process"
order = 100
enabled = true

[[passes]]
name = "MotionBlur"
phase = "post_process"
order = 200
enabled = false  # disabled by default, enable in settings

[[passes]]
name = "FXAA"
phase = "composite"
order = 0
enabled = true
```

### C.2 — Pass registry (name → factory)

**New files:** `engine/wayfinder/src/rendering/pipeline/RenderPassFactory.h`, `.cpp`

A registry that maps pass names to factory functions:

```
using RenderPassFactory = std::function<std::unique_ptr<RenderPass>()>;

class RenderPassRegistry
{
public:
    void Register(std::string_view name, RenderPassFactory factory);
    std::unique_ptr<RenderPass> Create(std::string_view name) const;
    bool Has(std::string_view name) const;
};
```

Engine registers built-in pass factories at startup. Game code registers its own. The pipeline config loader uses this to instantiate passes by name.

### C.3 — Pipeline config loader

**New files:** `engine/wayfinder/src/rendering/pipeline/PipelineConfig.h`, `.cpp`

```
struct PipelinePassEntry
{
    std::string PassName;
    RenderPhase Phase;
    int32_t Order = 0;
    bool Enabled = true;
};

struct PipelineConfig
{
    std::string Name;
    std::vector<PipelinePassEntry> Passes;
};

Result<PipelineConfig> LoadPipelineConfig(const std::filesystem::path& configPath);

/// Applies a pipeline config: creates passes from the factory registry and registers them. 
/// Engine-required passes (SceneOpaque, Debug, Composition) are always added even if not in the config.
void ApplyPipelineConfig(Renderer& renderer, const RenderPassRegistry& factory, const PipelineConfig& config);
```

### C.4 — Hot-reload support

Pipeline configs are loaded from disk → changing the TOML file and triggering a reload swaps the active pass set without recompiling. The flow:

1. File watcher detects change to `default.toml`.
2. Engine reloads config, diffs against current pass set.
3. Removes passes no longer listed, adds new ones, updates enabled/order for existing ones.
4. Next frame uses the new configuration.

This integrates with the existing hot-reload architecture (if any) or is wired up when that system exists.

### C.5 — Tests

- Load a pipeline config with 3 passes → verify correct phase/order assignment.
- Config with unknown pass name → warning logged, pass skipped.
- Config missing engine-required passes → they're still added.
- Config with `enabled = false` → pass is registered but disabled.
- Round-trip: load config → apply → get ordered list → verify against expected.

### C.6 — Relationship to dependency-driven injection (future-future)

When/if C++ gains reflection (or we add a code-generation step), `DeclareIntent()` can be auto-generated from schemas + pipeline configs. The TOML entries would gain optional dependency fields:

```toml
[[passes]]
name = "DepthOfField"
phase = "post_process"
order = 100
reads = ["SceneColour", "SceneDepth"]
after = ["Bloom"]  # soft dependency
```

The pipeline loader would build a dependency graph from these declarations and topologically sort passes for injection, eliminating the need for manual `order` values. This is the full data-driven dependency-driven model. The infrastructure from Phases A–C makes this a natural extension, not a rewrite.

---

## Decisions

- **Single unified pass list.** No engine/game split. All passes register with `(RenderPhase, order)`. Simple, predictable, scalable.
- **`RenderPhase` replaces `EngineRenderPhase`.** Rename reflects that game code uses the same phases. 8 phases covers everything from pre-opaque to present without being overly granular.
- **`PostProcessColour` is a convention, not a constraint.** Passes that follow the convention chain automatically. Passes that don't (e.g. they write their own named transient) still work — other passes just can't auto-discover them without knowing the name.
- **Breaking API change.** `AddPass(phase, order, pass)` replaces the old `AddPass(pass)`. Clean, explicit, no ambiguity about where a pass runs.
- **Schemas are metadata, not code generation.** The TOML schema describes an effect for tools/validation. The C++ struct is the runtime truth. Tests keep them in sync. No reflection or code gen needed.
- **Pipeline configs are optional.** The engine works without them — passes can be registered purely from C++ code. Configs add data-driven flexibility on top.
- **Phase C is independent of Phases A and B.** A and B are immediately implementable. C requires the pass factory registry and config loader but builds directly on A's unified pass list.

---

## Relevant Files

### Phase A — Modified
- [engine/wayfinder/src/rendering/pipeline/RenderPipeline.h](engine/wayfinder/src/rendering/pipeline/RenderPipeline.h) — `RenderPhase` enum, unified `PassSlot`, single `m_passes` vector, new `RegisterPass`, `BuildGraph` signature change
- [engine/wayfinder/src/rendering/pipeline/RenderPipeline.cpp](engine/wayfinder/src/rendering/pipeline/RenderPipeline.cpp) — `Initialise()` updated registrations, `BuildGraph()` single-loop, `RegisterPass` replaces `RegisterEnginePass`
- [engine/wayfinder/src/rendering/pipeline/Renderer.h](engine/wayfinder/src/rendering/pipeline/Renderer.h) — New `AddPass(phase, order, pass)`, remove old `AddPass(pass)`, remove `m_passes`
- [engine/wayfinder/src/rendering/pipeline/Renderer.cpp](engine/wayfinder/src/rendering/pipeline/Renderer.cpp) — `Render()` drops `m_passes` arg, `AddPass` delegates to pipeline, `Shutdown` iterates pipeline's list
- [engine/wayfinder/src/rendering/graph/RenderGraph.h](engine/wayfinder/src/rendering/graph/RenderGraph.h) — `GraphTextureId::PostProcessColour`, `GraphTextures::PostProcessColour`
- [engine/wayfinder/src/rendering/graph/RenderPass.h](engine/wayfinder/src/rendering/graph/RenderPass.h) — Update doc comments (no structural change)
- [engine/wayfinder/src/rendering/passes/CompositionPass.cpp](engine/wayfinder/src/rendering/passes/CompositionPass.cpp) — Use `ResolvePostProcessInput`
- [engine/wayfinder/src/rendering/passes/PresentSourceCopyPass.cpp](engine/wayfinder/src/rendering/passes/PresentSourceCopyPass.cpp) — Update to use post-process colour convention
- [docs/render_passes.md](docs/render_passes.md) — Full rewrite of registration and ordering sections
- [tests/rendering/RenderPassTests.cpp](tests/rendering/RenderPassTests.cpp) — Update for new API, add phase ordering tests
- [tests/rendering/RenderPipelineTests.cpp](tests/rendering/RenderPipelineTests.cpp) — Update for new API
- [tests/rendering/RenderGraphTests.cpp](tests/rendering/RenderGraphTests.cpp) — Add PostProcessColour tests
- [tests/rendering/SceneOpaquePassTests.cpp](tests/rendering/SceneOpaquePassTests.cpp) — Update `BuildGraph` call (no gamePasses arg)

### Phase A — New
- `engine/wayfinder/src/rendering/graph/PostProcessColourUtils.h` — `ResolvePostProcessInput()` helper (may be inlined into RenderGraph.h instead)

### Phase B — New
- `engine/wayfinder/schemas/effects/colour_grading.toml`
- `engine/wayfinder/schemas/effects/vignette.toml`
- `engine/wayfinder/schemas/effects/chromatic_aberration.toml`
- `engine/wayfinder/src/rendering/materials/EffectSchema.h`
- `engine/wayfinder/src/rendering/materials/EffectSchema.cpp`
- `tests/rendering/EffectSchemaTests.cpp`
- `docs/effect_schemas.md` (or section in render_passes.md)

### Phase C — New
- `config/pipelines/default.toml`
- `engine/wayfinder/src/rendering/pipeline/RenderPassFactory.h`
- `engine/wayfinder/src/rendering/pipeline/RenderPassFactory.cpp`
- `engine/wayfinder/src/rendering/pipeline/PipelineConfig.h`
- `engine/wayfinder/src/rendering/pipeline/PipelineConfig.cpp`
- Tests for factory, config loader, hot-reload

### Phase C — Modified
- `engine/wayfinder/src/rendering/pipeline/Renderer.h/.cpp` — Optional `LoadPipelineConfig` integration
- `engine/wayfinder/src/rendering/pipeline/RenderPipeline.h/.cpp` — Support for dynamic pass add/remove from config changes

---

## Verification

### Phase A
1. All existing tests pass with the new API (update call sites).
2. New phase-ordering test: register 4 passes at different phases → verify `AddPasses` call order matches `(Phase, Order, InsertSequence)`.
3. New PostProcessColour test: two passes both read/write `PostProcessColour` → graph compiles, second reads first's output.
4. `ResolvePostProcessInput` unit test: returns `SceneColour` when no `PostProcessColour` was written, `PostProcessColour` when present.
5. Composition reads post-process colour correctly: register a mock post-process pass that writes `PostProcessColour`, verify Composition's `ReadTexture` gets that handle.
6. Build all configs: `cmake --build --preset debug` passes.
7. Run `tools/lint.py` and `tools/tidy.py` — clean.
8. Run journey sandbox — visual parity with current rendering (no post-process effects active, so SceneColour → Composition path unchanged).

### Phase B
1. Schema loader unit test: load all engine schema files, verify field counts and types.
2. Struct↔schema sync tests: verify each engine effect's field names/types match its schema.
3. Validation test: feed known-bad JSON (out-of-range, wrong type, unknown field) → verify warnings/errors emitted.
4. Schema round-trip: load schema → generate default JSON from schema → deserialise to C++ struct → compare to `Identity()`.

### Phase C
1. Config loader test: load `default.toml` → verify pass names, phases, orders, enabled state.
2. Unknown pass name → warning, not crash.
3. Missing engine passes → auto-added.
4. Apply config → verify `Renderer` pass list matches expected order.
5. Hot-reload test: apply config A → apply config B → verify diff correctly adds/removes/updates passes.

---

## Critical Implementation Detail: FindHandle Must Return Latest

**Verified:** `RenderGraph::FindHandle` currently does a **forward** linear scan and returns the **first** match. `AllocateResource` (called by `CreateTransient`) always appends — it never overwrites.

This means if BloomPass creates `PostProcessColour` and then DOFPass creates another `PostProcessColour`, a third pass calling `FindHandle("PostProcessColour")` gets BloomPass's version, not DOFPass's.

**Required fix in Phase A:** Change `FindHandle` to do a **reverse** scan (return the **last** resource with a matching name). This is correct semantics: in a frame graph, the "current version" of a named resource is the most recently created one. This one-line change (`for (int i = size-1; i >= 0; --i)`) makes the `PostProcessColour` convention work automatically.

**Impact on existing code:** Currently no two resources share a name (SceneColour, SceneDepth, PresentSource are each created once). The forward vs reverse scan is irrelevant when names are unique, so this change is backward-compatible.

**Tests to add:** A dedicated test with two passes both writing `PostProcessColour` — verify `FindHandle` returns the second pass's handle.

---

## Further Considerations

1. **Pass removal at runtime.** The unified list must support `RemovePass<T>()` without invalidating iteration. Currently `m_passes` is only iterated in `BuildGraph` which runs once per frame, and removal happens outside the frame. This should remain fine but document the contract: no removal during `BuildGraph`.

2. **Thread safety of `RegisterPass` / `RemovePass`.** Currently single-threaded (main thread only). Document this. If multi-threaded registration is ever needed, the sorted vector would need a lock or a staging buffer — but YAGNI.
