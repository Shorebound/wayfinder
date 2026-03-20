# Wayfinder Engine — Improvement Plan

> Authored from a full codebase audit (March 2026). This document is the canonical reference for what to work on and in what order.

---

## How to Read This Plan

Each item has a **priority tier**, a **difficulty estimate**, and a **dependency list**. Work within a tier can generally be done in any order unless an explicit dependency is noted.

- **Must-Do (P1)** — Blocking correctness, safety, or fundamental architecture quality. Do these before building new features.
- **Should-Do (P2)** — Important for long-term health. Delaying these accumulates real debt.
- **Nice-to-Have (P3)** — Polish, DX improvements, and future-proofing. Do when convenient or when touching adjacent code.
- **Horizon (P4)** — Larger initiatives that depend on earlier tiers being substantially complete.

Difficulty: **S** (a focused session), **M** (a day or two), **L** (multiple days), **XL** (a week+).

---

## P1 — Must-Do

### P1.1: Test Coverage Expansion

**Difficulty: L  |  Dependencies: None  |  Priority: Highest**

The engine has 3 test files, all in `tests/rendering/`. Zero coverage for scene loading, ECS integration, state machine transitions, gameplay tags, asset resolution, component serialisation round-trips, or the module/plugin system. This is the single highest-risk gap — any refactor will be flying blind without tests.

#### What to Test

| Domain | Test File | Key Cases |
|---|---|---|
| Scene loading | `tests/scene/SceneLoadTests.cpp` | Load valid TOML → entities created with correct components; invalid TOML → graceful error; missing prefab → error reported; hierarchy (ChildOf) rebuilt correctly |
| Scene save round-trip | `tests/scene/SceneSaveTests.cpp` | Load → save → reload → compare entity count, component values, hierarchy, IDs preserved |
| Component serialisation | `tests/scene/ComponentSerializationTests.cpp` | Each component type: TOML → Apply → Serialize → compare. Cover all fields, defaults, edge cases (empty optional, zero scale) |
| Component validation | `tests/scene/ComponentValidationTests.cpp` | `ValidateFn` rejects bad data for each component type: wrong types, out-of-range values, missing required fields |
| ECS integration | `tests/scene/ECSIntegrationTests.cpp` | Entity create/destroy, component add/remove, SceneOwnership filtering, scene transition clears only scene entities |
| State machine | `tests/core/GameStateMachineTests.cpp` | Configure states → verify initial state; transition → verify current state; run conditions enable/disable systems correctly; unknown state → error |
| Gameplay tags | `tests/core/GameplayTagTests.cpp` | Tag creation, interning correctness, `IsChildOf` hierarchy, `GameplayTagContainer` (HasExact, HasTag, HasAny, HasAll), TOML tag file loading |
| Module / Plugin | `tests/core/ModuleRegistryTests.cpp` | Register systems/components/states/tags via ModuleRegistry; ApplyToWorld installs them; Plugin::Build delegates correctly |
| Subsystem collection | `tests/core/SubsystemTests.cpp` | Register → Initialise → Get; predicate gating (ShouldCreate=false skips); Shutdown in reverse order; missing subsystem asserts |
| Asset resolution | `tests/assets/AssetServiceTests.cpp` | BuildFromDirectory finds assets; ResolveRecord by ID; missing asset → nullptr; material loading from TOML |
| Identifiers | `tests/core/IdentifierTests.cpp` | UUID generate/parse round-trip; TypedId equality/hashing; StringHash constexpr correctness; FNV-1a collision resistance spot-check |
| InternedString | `tests/core/InternedStringTests.cpp` | Same string → same pointer; different strings → different pointers; empty string; thread safety (if applicable) |
| Handle & ResourcePool | `tests/core/HandleTests.cpp` | Handle validity, equality, hashing; pool acquire/release, generation bump, stale handle rejection, pool growth, ActiveCount |

#### Implementation Notes

- Use existing doctest framework. Each domain gets its own `.cpp` file.
- Scene/ECS tests need a headless `flecs::world` — no window or device required. Call `Scene::RegisterCoreECS(world)` to set up components/modules, then create `Scene{world, registry, "test"}`.
- State machine tests need a `flecs::world` + `ModuleRegistry` with test state descriptors.
- Tag tests: create `GameplayTagRegistry`, register tags programmatically and from test TOML.
- Asset tests: use a test fixture directory with known TOML files. Can live in `tests/fixtures/`.

#### CMake Changes

Update `tests/CMakeLists.txt` to add source files per new test `.cpp`. Consider grouping into multiple executables or one executable with all tests — doctest supports both. A single executable is simpler for now:

```cmake
add_executable(wayfinder_tests
  # Rendering
  rendering/RenderPipelineTests.cpp
  rendering/RenderGraphTests.cpp
  rendering/RenderFeatureTests.cpp
  # Scene
  scene/SceneLoadTests.cpp
  scene/SceneSaveTests.cpp
  scene/ComponentSerializationTests.cpp
  scene/ComponentValidationTests.cpp
  scene/ECSIntegrationTests.cpp
  # Core
  core/GameStateMachineTests.cpp
  core/GameplayTagTests.cpp
  core/ModuleRegistryTests.cpp
  core/SubsystemTests.cpp
  core/IdentifierTests.cpp
  core/InternedStringTests.cpp
  # Assets
  assets/AssetServiceTests.cpp
)
```

#### Definition of Done

- All listed test files exist and pass.
- `ctest --preset test` runs clean.
- Each test file covers at minimum the cases described in the table.
- No test requires a window, GPU device, or filesystem outside `tests/fixtures/`.

---

### P1.2: Generational Handle System

**Difficulty: M  |  Dependencies: None  |  Scope: Engine-wide**

All GPU resource handles (`GPUShaderHandle`, `GPUPipelineHandle`, `GPUBufferHandle`, `GPUTextureHandle`, `GPUSamplerHandle`, `GPUComputePipelineHandle`) are currently `void*`. This allows silent misuse — passing a texture where a sampler is expected compiles without error — and provides zero defence against use-after-free. Beyond rendering, the engine needs managed handles for physics bodies, audio instances, assets, UI widgets, and any other pooled resource.

Rather than a half-measure typed `void*` wrapper, implement **generational handles** from the start. Each handle carries an index + generation counter. The owning pool validates the generation on every access, catching use-after-free at runtime while the tag type catches wrong-type misuse at compile time.

#### Design

A single engine-wide `Handle<TTag>` template in `core/`, paired with a `ResourcePool<TTag, TResource>` that manages the index→resource mapping and validates generations on access.

```cpp
// core/Handle.h — engine-wide infrastructure

namespace Wayfinder
{
    template <typename TTag>
    struct Handle
    {
        uint32_t Index      : 20 = 0;
        uint32_t Generation : 12 = 0;

        [[nodiscard]] constexpr bool IsValid() const noexcept { return Generation != 0; }
        constexpr explicit operator bool() const noexcept { return IsValid(); }
        constexpr auto operator<=>(const Handle&) const = default;
        static constexpr Handle Invalid() noexcept { return {}; }
    };
}

// std::hash specialisation for unordered containers
template <typename TTag>
struct std::hash<Wayfinder::Handle<TTag>>
{
    size_t operator()(const Wayfinder::Handle<TTag>& h) const noexcept
    {
        return std::hash<uint32_t>{}(
            (static_cast<uint32_t>(h.Index) << 12) | h.Generation);
    }
};
```

Paired with a resource pool:

```cpp
// core/ResourcePool.h

namespace Wayfinder
{
    template <typename TTag, typename TResource>
    class ResourcePool
    {
    public:
        using HandleType = Handle<TTag>;

        HandleType Acquire(TResource&& resource);
        void Release(HandleType handle);

        [[nodiscard]] bool IsValid(HandleType handle) const;
        [[nodiscard]] TResource* Get(HandleType handle);
        [[nodiscard]] const TResource* Get(HandleType handle) const;
        [[nodiscard]] size_t ActiveCount() const;

    private:
        struct Entry
        {
            TResource Resource{};
            uint32_t Generation = 0;
            bool Alive = false;
        };

        std::vector<Entry> m_entries;
        std::vector<uint32_t> m_freeList;
    };
}
```

GPU-specific aliases stay thin:

```cpp
// rendering/GPUHandle.h — aliases only

struct GPUShaderTag {};
struct GPUPipelineTag {};
struct GPUBufferTag {};
struct GPUTextureTag {};
struct GPUSamplerTag {};
struct GPUComputePipelineTag {};

using GPUShaderHandle          = Handle<GPUShaderTag>;
using GPUPipelineHandle        = Handle<GPUPipelineTag>;
using GPUBufferHandle          = Handle<GPUBufferTag>;
using GPUTextureHandle         = Handle<GPUTextureTag>;
using GPUSamplerHandle         = Handle<GPUSamplerTag>;
using GPUComputePipelineHandle = Handle<GPUComputePipelineTag>;
```

`SDLGPUDevice` owns the pools internally — raw SDL pointers never leave the backend:

```cpp
// In SDLGPUDevice (private)
ResourcePool<GPUTextureTag, SDL_GPUTexture*> m_textures;
ResourcePool<GPUShaderTag, SDL_GPUShader*>   m_shaders;
ResourcePool<GPUBufferTag, SDL_GPUBuffer*>   m_buffers;
// etc.
```

#### Bit Split: 20 Index / 12 Generation

- **20-bit index** → ~1M concurrent resources per type. The codebase currently has ~10-100 active per GPU resource type. Even at engine-wide scale (mesh assets, texture atlases, audio clips, physics bodies), thousands is a realistic ceiling, not millions. This is well within budget.
- **12-bit generation** → 4095 reuses per slot before wraparound. A slot must be created and destroyed 4095 times *while a stale handle to that exact slot persists* to produce a false positive. Practically impossible under normal use.
- **Total: 32 bits** → fits in a register, trivially hashable, sorts naturally, serialisable, and `sizeof(Handle<T>) == 4`.

This is the same split used by EnTT, bgfx, and sokol_gfx. It's the industry standard for resource handles. If a future domain genuinely requires more (unlikely), a `Handle64<TTag>` variant with 32:32 split can be added alongside without disrupting existing code.

#### Engine-wide Applicability

The handle system lives in `core/`, not `rendering/`. Other domains will use it as they come online:

| Domain | Tag | Managed Resource |
|---|---|---|
| GPU Textures | `GPUTextureTag` | `SDL_GPUTexture*` |
| GPU Shaders | `GPUShaderTag` | `SDL_GPUShader*` |
| GPU Buffers | `GPUBufferTag` | `SDL_GPUBuffer*` |
| GPU Pipelines | `GPUPipelineTag` | `SDL_GPUGraphicsPipeline*` |
| GPU Samplers | `GPUSamplerTag` | `SDL_GPUSampler*` |
| Physics Bodies (P2.4) | `PhysicsBodyTag` | Jolt `BodyID` |
| Audio Instances (P4.5) | `AudioInstanceTag` | Audio source state |
| Mesh Assets (P4.1) | `MeshAssetTag` | Mesh data |
| Loaded Assets (P4.3) | `TextureAssetTag` | Texture metadata |

Each `ResourcePool` is owned by the subsystem that manages that resource type. The handle is the only thing that crosses subsystem boundaries.

#### Migration Steps

1. Create `core/Handle.h` with the template, `operator<=>`, `std::hash` specialisation.
2. Create `core/ResourcePool.h` with the template implementation.
3. Create `rendering/GPUHandle.h` with tag types and `using` aliases.
4. Remove `using GPUShaderHandle = void*` etc. from `RenderDevice.h` and `RenderTypes.h`.
5. Add `ResourcePool` members to `SDLGPUDevice` for each resource type.
6. Update `Create*()` methods to acquire from pool + return handle. Update `Destroy*()` to release.
7. Update `NullDevice` similarly (can use dummy pools or skip validation).
8. Update all call sites: `== nullptr` → `.IsValid()`, `= nullptr` → `= Handle::Invalid()` or `= {}`.
9. Update `PipelineCache`, `ShaderManager`, `TransientResourcePool`, `TransientBufferAllocator` to store/compare handles instead of raw pointers.
10. Add unit tests for `Handle` and `ResourcePool` (see P1.1 test table).
11. Ensure all existing render tests pass. Sandbox builds and runs.

#### Definition of Done

- `Handle<T>` and `ResourcePool<T, R>` exist in `core/` with full unit tests.
- No `void*` GPU handles remain in public API.
- Passing the wrong handle type is a compile error.
- Use-after-free is caught at runtime (generation mismatch → `Get()` returns `nullptr`).
- `ResourcePool` unit tests cover: acquire, release, re-acquire (generation bump), stale handle rejection, pool growth, `ActiveCount`.
- All existing tests pass. Sandbox builds and runs.

---

### P1.3: Use InternedString for RenderLayerId and RenderPassId

**Difficulty: S  |  Dependencies: None**

`RenderLayerId` and `RenderPassId` are currently `std::string`. They're used in equality comparisons on every mesh submission, every frame, for pass filtering. This is unnecessary allocation and comparison overhead for a fixed set of well-known values.

The engine already has `InternedString` (pointer-equality comparison) and `StringHash` (uint64 comparison). Either works; `InternedString` is preferred because it preserves debug-printable names while giving O(1) equality.

#### Migration

1. In `rendering/RenderIntent.h`, change the aliases:
   ```cpp
   // Before
   using RenderLayerId = std::string;
   using RenderPassId  = std::string;
   
   // After
   using RenderLayerId = InternedString;
   using RenderPassId  = InternedString;
   ```
2. Change well-known constants from `inline constexpr const char*` to `inline const InternedString`:
   ```cpp
   namespace RenderLayers
   {
       inline const InternedString Main  = InternedString::Intern("main");
       inline const InternedString Overlay = InternedString::Intern("overlay");
   }
   namespace RenderPassIds
   {
       inline const InternedString MainScene    = InternedString::Intern("main_scene");
       inline const InternedString OverlayScene = InternedString::Intern("overlay_scene");
       inline const InternedString Debug        = InternedString::Intern("debug");
   }
   ```
3. Fix all construction sites — anywhere doing `= std::string(RenderLayers::Main)` or `std::string(id)` becomes direct assignment.
4. Fix `RenderPass::AcceptsSceneSubmission` — the `==` comparison becomes pointer equality (fast).
5. Fix `RenderFrame::FindPass` — `pass.Id == id` becomes pointer equality.
6. Fix `RenderableComponent::Layer` default initialiser.

#### Files Touched

- `rendering/RenderIntent.h` (type aliases + constants)
- `rendering/RenderFrame.h` (RenderMeshSubmission, RenderPass, RenderFrame methods)
- `scene/Components.h` (RenderableComponent::Layer default)
- `rendering/SceneRenderExtractor.cpp` (layer mapping logic)
- `rendering/Renderer.cpp` (pass construction)
- Possibly `rendering/RenderPipeline.cpp`

#### Definition of Done

- `RenderLayerId` and `RenderPassId` are `InternedString`.
- No `std::string` allocation occurs for layer/pass identification during frame submission.
- All tests pass. Sandbox builds and runs.

---

### P1.4: Break Up Renderer

**Difficulty: L  |  Dependencies: P1.2 (recommended but not blocking)**

`Renderer` currently owns 11+ members and is responsible for shader management, pipeline caching, buffer allocation, transient resource pooling, debug pipeline setup, scene globals construction, render graph orchestration, feature management, and the actual render loop. This is a god object.

#### Target Decomposition

| New Type | Responsibility | Owns |
|---|---|---|
| `Renderer` (slimmed) | Frame orchestration only. Builds `RenderGraph`, calls `Compile()` + `Execute()`, manages feature lifecycle. | `RenderContext*`, `RenderPipeline*`, `std::vector<RenderFeature>` |
| `RenderContext` | Resource infrastructure. Created once at init, passed to anyone that needs to create GPU resources. | `ShaderManager`, `PipelineCache`, `ShaderProgramRegistry`, `TransientBufferAllocator`, `TransientResourcePool`, `GPUSamplerHandle(s)` |
| `RenderPipeline` (evolved) | Per-pass execution logic. Given a `RenderContext` + pass data, executes draw calls. | Nothing new — receives what it needs as params |

#### Migration Steps

1. **Create `RenderContext.h/cpp`**:
   ```cpp
   struct RenderContext
   {
       RenderDevice& Device;
       ShaderManager& Shaders;
       PipelineCache& Pipelines;
       ShaderProgramRegistry& Programs;
       TransientBufferAllocator& TransientBuffers;
       TransientResourcePool& TransientPool;
       GPUSamplerHandle NearestSampler;
   };
   ```
   This can start as a non-owning reference struct (members owned by `Renderer` during transition), then ownership can transfer later.
2. **Move shader program registration** out of `Renderer::Initialize()` into a dedicated setup function or into `RenderContext` init.
3. **Update `RenderFeatureContext`** to just carry a `RenderContext&` instead of individual members.
4. **Update `RenderPipeline`** to accept `RenderContext&` in its methods instead of getting individual pieces.
5. **Slim `Renderer`** — it should read approximately:
   ```cpp
   class Renderer
   {
   public:
       bool Initialize(RenderDevice& device, const EngineConfig& config);
       void Render(const RenderFrame& frame);
       void AddFeature(std::unique_ptr<RenderFeature> feature);
       // ...
   private:
       RenderContext m_context;    // or unique_ptr
       RenderPipeline m_pipeline;
       std::vector<std::unique_ptr<RenderFeature>> m_features;
       Mesh m_primitiveMesh;      // temporary until mesh asset system
       GPUPipeline m_debugLinePipeline; // temporary
   };
   ```

#### Phasing

This can be done incrementally:
1. First introduce `RenderContext` as a non-owning struct, populate it from `Renderer`'s existing members.
2. Pass it around. Nothing changes externally.
3. Gradually move ownership into `RenderContext`.
4. Remove redundant members from `Renderer`.

#### Definition of Done

- `Renderer::Render()` is < 50 lines — builds graph, compiles, executes.
- `RenderContext` holds all reusable GPU infrastructure.
- `RenderFeatureContext` wraps or extends `RenderContext`.
- No functional changes — same visual output, same tests passing.

---

### P1.5: Application Decomposition

**Difficulty: M  |  Dependencies: P1.4 (logically related)**

`Application` owns 8 unique_ptrs: `Module`, `ModuleRegistry`, `ProjectDescriptor`, `EngineConfig`, `Window`, `Input`, `Time`, `LayerStack`, `RenderDevice`, `Game`, `Renderer`, `SceneRenderExtractor`. The `Initialize()` method is a 100+ line chain. This makes the bootstrap path hard to test, hard to vary (headless mode, editor mode), and hard to reason about.

#### Target Decomposition

| Type | Responsibility |
|---|---|
| `Application` | Entry point. Owns the main loop, event dispatch, and high-level lifecycle (`Initialize` → `Run` → `Shutdown`). Owns the `EngineRuntime`. |
| `EngineRuntime` | Owns platform services (`Window`, `Input`, `Time`) and rendering services (`RenderDevice`, `Renderer`, `SceneRenderExtractor`). Provides a non-owning `EngineContext`. |
| `Game` | Unchanged — owns the ECS world and scene. |

#### Benefits

- `EngineRuntime` can be created without `Application` for integration tests.
- Editor (`Cartographer`) can create its own `EngineRuntime` with different configuration.
- Headless tools (`waypoint`, `navigator`) skip `EngineRuntime` entirely — they already do, but the boundary becomes explicit.

#### Migration Steps

1. Create `EngineRuntime.h/cpp` — move platform + rendering ownership from `Application`.
2. `Application::Initialize()` becomes: create `EngineRuntime`, create `Game`, wire them together.
3. `Application::Loop()` delegates to `EngineRuntime::BeginFrame()` / `EndFrame()` with `Game::Update()` in between.
4. `EngineContext` is built by `EngineRuntime` and handed to `Game::Initialize()`.

#### Definition of Done

- `Application` owns `EngineRuntime` + `Game` + `Module` (+ `LayerStack` if layers remain).
- `EngineRuntime` is independently constructible for test/editor use.
- All tests pass. Sandbox builds and runs.

---

## P2 — Should-Do

### P2.1: Scene Entity Index

**Difficulty: S  |  Dependencies: P1.1 (tests should exist first)**

`Scene::GetEntityById()` does an O(n) scan over all world entities via `m_world.each()`. For a scene with hundreds of entities this is measurably slow, and for editor operations (select by ID, resolve references) it's called frequently.

#### Design

Add an `std::unordered_map<SceneObjectId, flecs::entity>` index inside `Scene`:

```cpp
// Scene.h — add to private:
std::unordered_map<SceneObjectId, flecs::entity> m_entityIndex;
```

Maintain it:
- `CreateEntity()`: insert into index after setting `SceneObjectIdComponent`.
- `ClearEntities()`: clear the index.
- `LoadFromFile()`: index is populated as entities are created during load.

Query becomes O(1):
```cpp
Entity Scene::GetEntityById(const SceneObjectId& id)
{
    auto it = m_entityIndex.find(id);
    if (it == m_entityIndex.end()) return {};
    return Entity{it->second, this};
}
```

Requires `std::hash<SceneObjectId>` — delegate to `std::hash<Uuid>` which should hash the 128-bit value.

#### Definition of Done

- `GetEntityById` is O(1).
- Add test: create 100 entities, look up by ID, verify found.
- No scan of `m_world.each()` for ID lookup.

---

### P2.2: Decouple MeshComponent from Geometry

**Difficulty: M  |  Dependencies: None**

`MeshComponent` currently stores `MeshPrimitive Primitive` and `Float3 Dimensions`. This tightly couples the component to a specific geometry representation. When mesh assets are introduced, `MeshComponent` needs to reference an asset, not encode geometry parameters.

Additionally, `MaterialComponent` has a `Wireframe` bool that is a render state, not a material property.

#### Design

```cpp
// Geometry source: either a primitive or an asset reference
struct MeshComponent
{
    std::optional<AssetId> MeshAssetId;   // Future: loaded mesh assets
    MeshPrimitive Primitive = MeshPrimitive::Cube;  // Fallback for primitive meshes
    Float3 Dimensions = {1.0f, 1.0f, 1.0f};        // Only used with primitives
    
    bool IsAssetMesh() const { return MeshAssetId.has_value(); }
};

// Material: remove render state
struct MaterialComponent
{
    std::optional<AssetId> MaterialAssetId;
    Color BaseColor = Color::White();
    bool HasBaseColorOverride = false;
    // Wireframe REMOVED — becomes a per-entity render override or debug flag
};

// New: render overrides (optional component, added only when needed)
struct RenderOverrideComponent
{
    std::optional<bool> Wireframe;
    std::optional<CullMode> CullOverride;
};
```

#### Migration

1. Move `Wireframe` / `HasWireframeOverride` from `MaterialComponent` to `RenderOverrideComponent`.
2. Update `SceneRenderExtractor` to check for `RenderOverrideComponent` when building `RenderFillMode`.
3. Update `ComponentRegistry` entries for serialisation/deserialisation.
4. Update scene TOML files that use `wireframe` under `[material]` — move to `[render_override]` section.
5. Update existing tests and add new component serialisation tests.

#### Definition of Done

- `MaterialComponent` has no render-state fields.
- `MeshComponent` supports both primitive and asset paths.
- Wireframe is an opt-in override component.
- Scene files updated. Serialisation round-trips pass.

---

### P2.3: Frame-Linear Allocator for Render Graph

**Difficulty: M  |  Dependencies: P1.4 (RenderContext)**

The render graph is rebuilt every frame. Each `AddPass()` allocates `std::string` names, `std::vector` dependencies, and `std::function` closures. These are freed at end-of-frame and reallocated next frame — classic allocation churn.

#### Design

Introduce a frame-linear (bump/arena) allocator:

```cpp
// rendering/FrameAllocator.h

class FrameAllocator
{
public:
    explicit FrameAllocator(size_t initialCapacity = 64 * 1024);  // 64KB
    
    void* Allocate(size_t bytes, size_t alignment = alignof(std::max_align_t));
    
    template <typename T, typename... TArgs>
    T* New(TArgs&&... args);
    
    /// Reset for next frame. No destructors called — types must be trivially destructible
    /// or manually destroyed before reset.
    void Reset();
    
    size_t GetUsedBytes() const;
    size_t GetCapacity() const;

private:
    std::vector<uint8_t> m_buffer;
    size_t m_offset = 0;
};
```

#### Usage in RenderGraph

- `RenderGraph` takes a `FrameAllocator&` at construction.
- Pass names become `std::string_view` pointing into allocator memory (or use `InternedString` from P1.3).
- Resource entries use allocator storage.
- `std::function` replaced with a type-erased callable stored in the arena.

#### Phasing

1. **Phase A**: Implement `FrameAllocator` with tests (allocate, align, reset, grow).
2. **Phase B**: Convert `RenderGraph` pass names from `std::string` to `std::string_view` (assuming P1.3 lands InternedString for pass IDs — these are already pointer-stable).
3. **Phase C**: Replace `std::function<RenderGraphExecuteFn>` with a lightweight type-erased wrapper backed by the arena. This is the biggest win — `std::function` heap-allocates for any non-trivial capture. The wrapper stores a pair of function pointers (`InvokeFn`, `DestroyFn`) alongside the captured state, which is placement-new'd into the arena. On `Reset()`, the allocator walks a registered destructor list and calls each `DestroyFn` before bumping the pointer back. This allows non-trivially-destructible captures (e.g. spans, handles) without heap allocation.
4. **Phase D**: Profile to verify improvement. Measure allocations-per-frame before/after.

#### Definition of Done

- `FrameAllocator` exists with unit tests.
- Render graph construction causes zero heap allocations for pass names and execute functions.
- `FrameAllocator::Reset()` called once per frame.
- Tracy annotation on allocator usage for profiling.

---

### P2.4: Implement a Non-Rendering Subsystem End-to-End

**Difficulty: L  |  Dependencies: P1.1 (tests)**

The engine links Box2D and Jolt but has zero physics code. The `physics/` directory is empty. Implementing one non-rendering domain end-to-end validates the subsystem architecture, module registration, ECS integration, and data pipeline. Physics is the most natural candidate.

#### Scope

This is NOT a full physics engine integration. It's a thin proof-of-concept that:
1. Registers a physics subsystem via the module system.
2. Defines a `RigidBodyComponent` and `ColliderComponent`.
3. Runs a Flecs system that steps the physics world.
4. Syncs physics transforms back to `WorldTransformComponent`.

#### Design Sketch

```
engine/wayfinder/src/physics/
  PhysicsSubsystem.h/cpp     — GameSubsystem, owns Jolt PhysicsSystem
  PhysicsComponents.h        — RigidBodyComponent, ColliderComponent
  PhysicsModuleRegistry.cpp  — Registers ECS systems for physics step + sync
```

```cpp
// PhysicsComponents.h
struct RigidBodyComponent
{
    enum class Type { Static, Dynamic, Kinematic };
    Type BodyType = Type::Dynamic;
    float Mass = 1.0f;
    float LinearDamping = 0.01f;
    float AngularDamping = 0.05f;
    // Runtime: managed handle from P1.2 generational handle system
    Handle<PhysicsBodyTag> InternalBodyId{};
};

struct ColliderComponent
{
    enum class Shape { Box, Sphere, Capsule };
    Shape ColliderShape = Shape::Box;
    Float3 HalfExtents = {0.5f, 0.5f, 0.5f};
    float Radius = 0.5f;
    float Height = 1.0f;
};
```

#### ECS Systems

- `PhysicsStep` (FixedUpdate or PreUpdate): Step the Jolt world by `deltaTime`.
- `PhysicsSyncTransforms` (PreUpdate, after step): Read body transforms from Jolt, write to `WorldTransformComponent`.
- `PhysicsCreateBodies` (OnAdd observer): When `RigidBodyComponent` is added to an entity, create a Jolt body.
- `PhysicsDestroyBodies` (OnRemove observer): Clean up Jolt body on component removal.

#### Tests

- `tests/physics/PhysicsIntegrationTests.cpp`: Create world, add entity with RigidBody + Collider, step N times, verify position changed. Verify static bodies don't move. Verify removal cleans up.

#### Definition of Done

- `PhysicsSubsystem` registered and runs in the sandbox.
- Physics bodies created from scene entities.
- Transform sync works (physics → WorldTransformComponent).
- At least 5 integration tests pass.
- Architecture validated: module system → subsystem → ECS systems → component data → runtime state.

---

### P2.5: Blend State Support in Pipeline

**Difficulty: M  |  Dependencies: P1.2 (GPU handles)**

`PipelineCreateDesc` has no blend state — only `CullMode`, `FillMode`, `DepthTest`, and `DepthWrite`. There is a TODO comment in the struct ("Stage 6: Blend state, depth format, multiple color targets"). Without blend state, transparent rendering, additive particles, and alpha blending are impossible.

#### Design

```cpp
// rendering/RenderDevice.h — add to PipelineCreateDesc

enum class BlendFactor : uint8_t
{
    Zero,
    One,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    SrcColor,
    OneMinusSrcColor,
};

enum class BlendOp : uint8_t
{
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

struct BlendState
{
    bool Enabled = false;
    BlendFactor SrcColorFactor = BlendFactor::SrcAlpha;
    BlendFactor DstColorFactor = BlendFactor::OneMinusSrcAlpha;
    BlendOp ColorOp = BlendOp::Add;
    BlendFactor SrcAlphaFactor = BlendFactor::One;
    BlendFactor DstAlphaFactor = BlendFactor::OneMinusSrcAlpha;
    BlendOp AlphaOp = BlendOp::Add;
};
```

Provide preset factory functions for common modes:

```cpp
namespace BlendPresets
{
    constexpr BlendState Opaque() { return {}; }  // Enabled=false
    constexpr BlendState AlphaBlend()
    {
        return {true, BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha, BlendOp::Add,
                      BlendFactor::One,      BlendFactor::OneMinusSrcAlpha, BlendOp::Add};
    }
    constexpr BlendState Additive()
    {
        return {true, BlendFactor::SrcAlpha, BlendFactor::One, BlendOp::Add,
                      BlendFactor::SrcAlpha, BlendFactor::One, BlendOp::Add};
    }
    constexpr BlendState Premultiplied()
    {
        return {true, BlendFactor::One, BlendFactor::OneMinusSrcAlpha, BlendOp::Add,
                      BlendFactor::One, BlendFactor::OneMinusSrcAlpha, BlendOp::Add};
    }
}
```

#### Migration Steps

1. Add `BlendFactor`, `BlendOp`, `BlendState` enums/struct to `RenderDevice.h`.
2. Add `BlendState blend{}` to `PipelineCreateDesc`.
3. Map to SDL_GPU blend state in `SDLGPUDevice::CreatePipeline()`.
4. Add blend mode to `ShaderProgramDesc` so materials can declare their blend mode.
5. Update `PipelineCache` hash to include blend state fields.
6. Update `SortKey` — the transparent layer already exists, but submissions need blend mode awareness.
7. Remove the "Stage 6" TODO comment.

#### Definition of Done

- `PipelineCreateDesc` has a `BlendState` field.
- SDL_GPU backend correctly creates blended pipelines.
- An alpha-blended material renders correctly in the sandbox.
- Pipeline cache differentiates opaque vs blended pipelines.

---

### P2.6: Evolve or Remove RenderPipeline

**Difficulty: S  |  Dependencies: P1.4**

`RenderPipeline` currently has a single method: `bool Prepare(RenderFrame& frame) const`. It validates/sorts the frame's passes. With the render graph doing topological ordering and pass culling, `RenderPipeline::Prepare` is vestigial — the graph handles execution ordering.

#### Options

**Option A: Remove** — Inline the validation into `Renderer::Render()` or `RenderGraph::Compile()`. Delete `RenderPipeline.h/cpp`.

**Option B: Evolve** — Make `RenderPipeline` the owner of render graph construction policy. It decides which passes to add based on the `RenderFrame` contents, rather than `Renderer::Render()` doing this inline.

Recommendation: **Option B**, because as the engine grows (more render features, conditional passes, debug overlays), having a dedicated object that maps `RenderFrame` → `RenderGraph` is the right abstraction. But keep it thin until complexity demands more.

#### Definition of Done

- Either `RenderPipeline` is removed, or it owns `RenderGraph` construction.
- No orphaned code.

---

### P2.7: Error Handling Strategy

**Difficulty: M  |  Dependencies: None**

The engine uses a mix of `bool` returns, `std::optional`, and `std::string& error` out-params. There's no consistent pattern. Some failures are silent. This matters most at system boundaries (file loading, GPU resource creation, asset resolution).

#### Design

Introduce a lightweight `Result<T, E>` type:

```cpp
// core/Result.h
template <typename T, typename E = std::string>
class Result
{
public:
    static Result Ok(T value) { return Result{std::move(value)}; }
    static Result Err(E error) { return Result{std::unexpected(std::move(error))}; }
    // or use std::expected<T, E> (C++23)
    
    bool IsOk() const;
    bool IsErr() const;
    const T& Value() const;
    T&& TakeValue();
    const E& Error() const;
};
```

Actually, C++23 provides `std::expected<T, E>` — just use that directly with a project alias:

```cpp
// core/Result.h
#include <expected>

namespace Wayfinder
{
    template <typename T, typename E = std::string>
    using Result = std::expected<T, E>;
}
```

#### Migration (Incremental)

Don't rewrite everything at once. Apply `Result` at system boundaries as they're touched:

1. `RenderDevice::CreateShader` → `Result<GPUShaderHandle>`
2. `AssetService::LoadMaterialAsset` → `Result<const MaterialAsset*>`
3. `Scene::LoadFromFile` → `Result<void>` (or `Result<bool>` → just `Result<void, std::string>`)
4. `EngineConfig::Load` → `Result<EngineConfig>`
5. New code uses `Result` by default.

#### Definition of Done

- `Result` alias exists in `core/Result.h`.
- At least 3 system-boundary APIs migrated.
- Pattern documented in project coding guidelines.

---

### P2.8: Event Queue (Deferred Dispatch)

**Difficulty: M  |  Dependencies: None**

The event system currently uses synchronous blocking dispatch — `Event.h` itself notes this as a known limitation. Every event is handled the instant it fires, which prevents batching, makes deterministic replay impossible, and couples the dispatch callsite to every handler's execution time.

#### Design

- `EventQueue` class backed by `std::vector<std::unique_ptr<Event>>`.
- `Push(event)` during SDL polling; `Drain()` at a defined frame point.
- `Application` decides dispatch timing: immediate for latency-sensitive events (window close), queued for input events that benefit from batching.
- Queue is drained once per frame at a well-defined point in the update loop.

#### Benefits

- Deterministic replay / networking: record the queue, play it back.
- Profiling: one clear span for "event processing" per frame.
- Decouples event producers from handler execution order.

#### Definition of Done

- `EventQueue` exists with `Push()` and `Drain()` API.
- `Application::Loop()` uses the queue for input events.
- Latency-sensitive events (window close, resize) still dispatch immediately.
- Existing event handlers work unchanged.

---

## P3 — Nice-to-Have

### P3.1: TOML Hot-Reload via File Watching

**Difficulty: L  |  Dependencies: P2.7 (error handling for reload errors)**

The engine loads all configuration and scene data at startup with no mechanism for runtime reloading. For a data-driven engine targeting rapid iteration, this is a significant DX gap.

#### Design

```
engine/wayfinder/src/platform/
  FileWatcher.h          — Interface: Watch(path, callback), Poll()
  FileWatcher.cpp        — Platform dispatch (Windows → ReadDirectoryChangesW)
  win32/
    Win32FileWatcher.cpp — ReadDirectoryChangesW implementation
```

#### Scope (Phase 1 — Config Hot-Reload)

- Watch `config/engine.toml` and `config/tags/*.tags.toml`.
- On change: debounce (coalesce events within ~100ms to handle editors that write multiple times per save), then re-parse, validate, apply deltas.
- `EngineConfig` gets a `ReloadFrom(path)` method.
- `GameplayTagRegistry` gets a `ReloadTagFiles()` method.
- Changes take effect next frame — not mid-frame.

#### Scope (Phase 2 — Asset Hot-Reload)

- Watch asset directories (materials, prefabs).
- On material change: invalidate `RenderResourceCache` entry, reload from disk.
- On prefab change: mark affected `PrefabInstanceComponent` entities dirty, re-apply.
- Scene files: manual reload (too disruptive for automatic).

#### Scope (Phase 3 — Shader Hot-Reload)

- Watch shader source directory.
- On `.hlsl`/`.glsl` change: recompile to SPIR-V, invalidate pipeline cache entries, recreate pipelines.
- This requires the shader compiler to be available at runtime, or a background compilation step.

#### Definition of Done (Phase 1)

- `FileWatcher` polled once per frame in `Application::Loop()`.
- Config changes applied within 1 frame of file save.
- No crash on malformed TOML — error logged, old config retained.
- Manual trigger also available: `Game::ReloadConfig()`.

---

### P3.2: Explicit Source File Lists in CMake

**Difficulty: S  |  Dependencies: None**

The engine uses `GLOB_RECURSE` for source discovery:
```cmake
file(GLOB_RECURSE ENGINE_ALL_SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.h")
```

This is fragile — `CONFIGURE_DEPENDS` re-globs on every configure (slow), and some generators don't support it reliably. Explicit lists are the CMake-recommended approach and make it immediately clear what's compiled.

#### Migration

Generate the initial list from the current glob:
```powershell
Get-ChildItem -Recurse engine/wayfinder/src -Include *.cpp, *.h | 
  ForEach-Object { $_.FullName.Replace("$PWD\engine\wayfinder\", "").Replace("\", "/") } | 
  Sort-Object
```

Paste the result into `target_sources()` blocks grouped by directory. Use CMake `source_group()` for IDE folder structure.

#### Trade-off

This adds friction when creating new files: you must add them to CMakeLists.txt. But it eliminates the "I added a file and it didn't get picked up" class of bugs, and it makes the build deterministic.

The existing repo memory notes: *"GLOB_RECURSE for sources — new files auto-detected on CMake reconfigure"* and *"adding new source files may require a manual configure step"* — explicit lists would make this explicit rather than surprising.

#### Definition of Done

- No `file(GLOB_RECURSE)` for source files.
- All source files listed explicitly.
- Build works identically.

---

### P3.3: clang-tidy Integration

**Difficulty: S  |  Dependencies: None**

The project has `.clang-format` but no static analysis. clang-tidy catches bugs, enforces modernisation, and can be integrated into CI.

#### Setup

1. Create `.clang-tidy` at workspace root:
   ```yaml
   Checks: >
     -*,
     bugprone-*,
     -bugprone-easily-swappable-parameters,
     cppcoreguidelines-*,
     -cppcoreguidelines-avoid-magic-numbers,
     -cppcoreguidelines-pro-type-reinterpret-cast,
     modernize-*,
     -modernize-use-trailing-return-type,
     performance-*,
     readability-identifier-naming

   CheckOptions:
     - key: readability-identifier-naming.ClassCase
       value: CamelCase
     - key: readability-identifier-naming.MemberPrefix
       value: m_
     - key: readability-identifier-naming.ConstantCase
       value: UPPER_CASE
   ```
2. Enable in CMake:
   ```cmake
   set(CMAKE_CXX_CLANG_TIDY "clang-tidy;--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy")
   ```
   Or run manually / in CI:
   ```bash
   clang-tidy -p build engine/wayfinder/src/**/*.cpp
   ```
3. Fix initial findings (expect noise — suppress with `NOLINT` only for genuine false positives).

#### Definition of Done

- `.clang-tidy` config committed.
- Clean run on engine sources (warnings acknowledged or suppressed with reason).
- CI step runs clang-tidy on PRs (once CI exists).

---

### P3.4: Scene Prefab Instantiation API

**Difficulty: M  |  Dependencies: P1.1 (tests), P2.2 (mesh/material split)**

Prefabs are loaded during `SceneDocument` parsing but there's no runtime API to instantiate a prefab programmatically. Game code that wants to spawn entities from prefabs has to go through the scene file.

#### Design

```cpp
// Scene.h — new method
Entity Scene::InstantiatePrefab(const AssetId& prefabAssetId, const TransformComponent& transform = {});
```

Implementation:
1. Resolve prefab path via `AssetService`.
2. Parse prefab TOML via `SceneDocument` (single-entity parse).
3. Create entity, apply components, set transform override.
4. Return the new `Entity`.

For nested prefabs (prefab references another prefab), resolve recursively with a **visited set** (to detect cycles) and a depth limit (sanity cap, e.g. 32). A cycle produces a clear error naming the chain, not a stack overflow.

#### Definition of Done

- `Scene::InstantiatePrefab()` works for all component types.
- Prefab override values apply correctly.
- Test: instantiate prefab, verify component values match definition + overrides.

---

### P3.5: RenderMeshHandle Type Safety

**Difficulty: XS  |  Dependencies: P1.2 (generational handles)**

With the generational handle system from P1.2, this becomes trivial: define a `RenderMeshTag` and alias `using RenderMeshHandle = Handle<RenderMeshTag>`. The `ResourcePool<RenderMeshTag, MeshData>` can be owned by the renderer or a future mesh manager (P4.1). No special design work needed — it falls out naturally from the handle infrastructure.

#### Definition of Done

- `RenderMeshHandle` is `Handle<RenderMeshTag>` — a distinct type from all GPU handles.
- Compile-time safety: can't pass a `RenderMeshHandle` where a `GPUBufferHandle` is expected or vice versa.

---

### P3.6: GPU Debug Annotations

**Difficulty: S  |  Dependencies: None**

The engine links Tracy but has no GPU-side debug markers. GPU profilers (RenderDoc, Nsight, Tracy GPU zones) rely on `push_debug_group` / `pop_debug_group` calls to label work on the timeline. Without them, GPU captures show unlabelled draw calls — useless for diagnosing render performance.

#### Design

Add two methods to `RenderDevice`:

```cpp
// rendering/RenderDevice.h

virtual void PushDebugGroup(std::string_view label) = 0;
virtual void PopDebugGroup() = 0;
```

And an RAII scope helper:

```cpp
struct GPUDebugScope
{
    RenderDevice& Device;
    GPUDebugScope(RenderDevice& device, std::string_view label) : Device(device) { Device.PushDebugGroup(label); }
    ~GPUDebugScope() { Device.PopDebugGroup(); }
};
```

#### Backend Implementation

SDL_GPU: Use `SDL_InsertGPUDebugLabel` / `SDL_PushGPUDebugGroup` / `SDL_PopGPUDebugGroup`.

`NullDevice`: no-ops.

#### Integration Points

- `RenderGraph::Execute()` wraps each pass execution in a debug group using the pass name.
- `Renderer::Render()` wraps the entire frame in a top-level group.
- `RenderFeature::AddPasses()` — features can optionally annotate their own sub-work.

#### Definition of Done

- `RenderDevice` has `PushDebugGroup` / `PopDebugGroup`.
- SDL_GPU backend calls the SDL debug label API.
- Render graph pass execution is annotated.
- RenderDoc captures show labelled pass names.

---

### P3.7: Multiple Render Targets (MRT) in Render Graph

**Difficulty: M  |  Dependencies: P2.5 (blend state), P1.4 (RenderContext)**

The render graph currently supports a single `ColorWriteInfo` and a single `DepthWriteInfo` per pass. Deferred rendering, G-buffer generation, and techniques like velocity buffer output require writing to multiple colour attachments simultaneously.

#### Design

Change `RenderGraphBuilder` and `PassEntry`:

```cpp
// Before
std::optional<ColorWriteInfo> ColorWrite;

// After
std::vector<ColorWriteInfo> ColorWrites;  // index = attachment slot
```

`RenderGraphBuilder` gains:

```cpp
void WriteColor(RenderGraphHandle handle, uint32_t slot = 0, LoadOp load = LoadOp::Clear, ClearValue clear = {});
```

During execution, the render pass descriptor is built with N colour attachments matching the pass's `ColorWrites` vector.

#### SDL_GPU Support

`SDL_BeginGPURenderPass` already accepts an array of `SDL_GPUColorTargetInfo` — this maps directly.

#### Definition of Done

- Render graph passes can declare multiple colour writes.
- `RenderGraph::Execute()` creates render passes with the correct attachment count.
- Existing single-attachment passes continue to work unchanged.
- Add a render graph test that exercises a 2-attachment pass.

---

### P3.8: Upload Batching for TransientBufferAllocator

**Difficulty: M  |  Dependencies: P2.3 (frame allocator)**

Each `UploadToBuffer` call currently creates a staging transfer buffer, acquires a dedicated command buffer, begins a copy pass, uploads, ends the pass, submits, and releases the transfer buffer. This is O(N) command buffer submissions for N transient allocations per frame. With 1–2 allocations (debug lines, grid) this is fine, but it will become a bottleneck with particles, text quads, or many dynamic geometry sources.

#### Design

- Queue all staging writes during the frame into a single large staging buffer.
- At a defined flush point (after all `Allocate` calls, before rendering), submit one command buffer with one copy pass containing all uploads.
- Alternatively: use persistent buffer mapping (if SDL_GPU exposes it) to skip staging entirely for transient data.

#### When

When profiling shows upload overhead matters — particles, lots of dynamic geometry, or many transient allocations per frame.

#### Definition of Done

- Single command buffer submission per frame for all transient uploads.
- Profiling confirms reduced overhead compared to per-allocation submission.
- Existing transient allocation API unchanged for callers.

---

### P3.9: Sub-Sort Key Utilization

**Difficulty: S  |  Dependencies: None**

The 64-bit sort key reserves 14 bits for sub-sort (tiebreaker), but currently only uses `SortPriority` (a `uint8_t`, 0–255). The extra 6 bits of headroom are unused. Potential uses: entity ID hash for deterministic ordering across frames, sequence counter for submission-order stability.

#### When

When non-deterministic draw ordering causes visible artifacts (shimmer, Z-fighting between co-planar meshes).

#### Design

- Hash the entity ID or use a frame-monotonic sequence counter to fill the remaining sub-sort bits.
- Ensures draw order is deterministic across frames for identical sort keys.

#### Definition of Done

- Sub-sort field uses all 14 bits with a deterministic tiebreaker.
- No visible ordering flicker between objects sharing the same pipeline, material, and depth.

---

## P4 — Horizon

These are larger initiatives that build on earlier tiers. They're documented here for planning visibility but shouldn't be started until their dependencies are complete.

### P4.1: Mesh Asset System

**Dependencies: P2.2, P2.4 (validates data pipeline), P1.2**

The engine currently has only `MeshPrimitive::Cube` with hardcoded geometry. A real mesh asset system needs:

- Mesh file format (glTF import → engine-native binary format).
- Mesh loading in `AssetService` (similar to `MaterialAsset`).
- `MeshComponent` references mesh by `AssetId`.
- `RenderResourceCache` caches uploaded GPU buffers for meshes.
- Multiple vertex formats per mesh (position-only for shadow pass, full for lit pass).

This is a large feature. Sketch it out in detail before starting.

---

### P4.2: Composable Vertex Attribute System

**Dependencies: P4.1 (mesh asset system)**

The engine uses hardcoded vertex structs (`VertexPosColor`, `VertexPosNormalUV`, etc.) with pre-built `VertexLayout` instances. This breaks down when mesh assets arrive — imported meshes may have arbitrary combinations of positions, normals, tangents, multiple UV sets, vertex colours, skinning weights.

#### Design

Replace the fixed set of vertex structs with a composable attribute system:

```cpp
enum class VertexSemantic : uint8_t
{
    Position,       // vec3
    Normal,         // vec3
    Tangent,        // vec4 (xyz = tangent, w = handedness)
    Color0,         // vec4 (vertex color)
    TexCoord0,      // vec2
    TexCoord1,      // vec2
    Joints,         // uvec4 (skinning)
    Weights,        // vec4 (skinning)
};

struct VertexAttributeDesc
{
    VertexSemantic Semantic;
    VertexAttribFormat Format;
    uint32_t Offset;
};

struct VertexLayoutDesc
{
    std::vector<VertexAttributeDesc> Attributes;
    uint32_t Stride = 0;  // 0 = auto-calculate from attributes
};
```

#### Benefits

- Mesh importer produces a `VertexLayoutDesc` matching the source data.
- Shader programs declare which semantics they consume.
- The renderer can validate shader-mesh compatibility at bind time.
- Shadow passes can use position-only sub-layouts without duplicating mesh data.

#### Migration

1. Introduce `VertexSemantic` and `VertexLayoutDesc`.
2. Keep existing `VertexLayouts::PosColor` etc. as pre-built `VertexLayoutDesc` instances.
3. Update `ShaderProgramDesc` to declare required semantics.
4. Update pipeline creation to build `VertexLayout` from `VertexLayoutDesc`.
5. Mesh asset loader (P4.1) produces `VertexLayoutDesc` per imported mesh.

#### Definition of Done

- `VertexLayoutDesc` describes arbitrary vertex formats.
- Existing hardcoded layouts expressed as `VertexLayoutDesc` instances.
- Pipeline creation works with both old and new layouts.

---

### P4.3: Texture Asset Pipeline

**Dependencies: P1.2 (GPU handles), P2.7 (error handling)**

The engine can create GPU textures (`RenderDevice::CreateTexture`) but has no pipeline for loading textures from disk. Materials reference shaders and parameters, but not texture maps (diffuse, normal, roughness, etc.). Any shader that samples a texture needs this.

#### Scope

| Component | Purpose |
|---|---|
| `TextureAsset` | Authored definition in TOML or directly from image path |
| `TextureLoader` | Disk → CPU pixels (stb_image or SDL_image) |
| `TextureManager` | CPU pixels → `GPUTextureHandle`, caching by asset ID |
| `MipGenerator` | Optional — generate mip chain on CPU or via compute pass |

#### Design Sketch

```cpp
struct TextureAsset
{
    AssetId Id;
    std::string Name;
    std::filesystem::path SourcePath;
    TextureFormat Format = TextureFormat::RGBA8_UNORM;
    SamplerFilter Filter = SamplerFilter::Linear;
    SamplerAddressMode AddressMode = SamplerAddressMode::Repeat;
    bool GenerateMips = true;
};

class TextureManager
{
public:
    GPUTextureHandle Load(const AssetId& id);
    GPUTextureHandle GetOrDefault(const AssetId& id);  // returns pink checkerboard if missing
    void Reload(const AssetId& id);  // for hot-reload
    void ReleaseAll();
private:
    std::unordered_map<AssetId, GPUTextureHandle> m_cache;
};
```

#### Material Integration

Extend `MaterialParameterBlock` with texture parameter support:

```cpp
enum class MaterialParamType : uint8_t
{
    Float, Vec2, Vec3, Vec4, Color, Int,
    Texture,  // NEW — value is AssetId, bound as sampler+texture
};
```

Material TOML gains a `[textures]` section:

```toml
[textures]
diffuse = "textures/character_diffuse.png"
normal = "textures/character_normal.png"
```

#### Definition of Done

- Textures loadable from PNG/TGA/BMP via `TextureManager`.
- Materials can reference textures by path or asset ID.
- Shader programs can declare texture sampler slots.
- Missing textures get a visible fallback (pink checkerboard).
- At least one material in the sandbox uses a texture.

---

### P4.4: Debug Tooling Subsystem (ImGui)

**Dependencies: P1.5 (Application decomposition), P1.4 (RenderContext)**

Dear ImGui is linked but unused. The engine needs a debug overlay system:

- `DebugOverlaySubsystem` — GameSubsystem that owns ImGui context.
- Render feature: `ImGuiRenderFeature` that injects an ImGui pass into the render graph (after composition).
- Debug panels: FPS, entity inspector, component editor, render graph visualiser, physics debug.
- Layer-based: debug UI is a `Layer` in the `LayerStack`, receives events.

---

### P4.5: Audio Subsystem

**Dependencies: P2.4 (validates subsystem architecture)**

The `audio/` directory is empty. Needs:

- Audio library selection (miniaudio, FMOD, or SDL3_mixer).
- `AudioSubsystem` — GameSubsystem, owns audio device.
- `AudioSourceComponent` — spatial audio emitter.
- `AudioListenerComponent` — typically on the camera entity.
- Data-driven: sound bank definitions in TOML.

---

### P4.6: Scripting System

**Dependencies: Core architecture stable (P1 + P2 complete)**

The `scripting/` directory is empty. Options:

- **Lua** (via sol2): Lightweight, proven in games, hot-reloadable.
- **C# (via .NET host)**: Heavier, but more familiar to game developers. 
- **Angelscript**: Purpose-built for games, good C++ interop.

Whatever the choice, scripts should:
- Defined as components (`ScriptComponent` with asset reference).
- Executed by ECS systems.
- Have access to entity/component queries via a sandboxed API.
- Be hot-reloadable.

---

### P4.7: Editor (Cartographer) Bootstrap

**Dependencies: P1.5 (EngineRuntime), P4.4 (ImGui), P2.2 (component model clean)**

The `apps/cartographer/` directory is empty. Initial editor needs:

- Standalone application that creates `EngineRuntime` with editor configuration.
- Scene viewport (renders via engine).
- Entity hierarchy panel.
- Component inspector (driven by `ComponentRegistry` metadata).
- Property editing backed by the same serialisation system as scene files.
- Undo/redo (command pattern over component mutations).

This is a major initiative. Document its design separately in `docs/plans/editor_design.md` before starting.

---

### P4.8: CI Pipeline

**Dependencies: P1.1 (tests), P3.3 (clang-tidy)**

No CI exists. Minimum viable pipeline:

- **Build**: `cmake --preset dev` + `cmake --build --preset debug` on Windows (MSVC).
- **Test**: `ctest --preset test`.
- **Lint**: clang-format check + clang-tidy.
- **Asset validation**: `waypoint validate-assets sandbox/journey/assets`.
- Platform: GitHub Actions (since the repo is on GitHub).

---

### P4.9: Data-Driven Input Action Mapping

**Dependencies: P2.8 (event queue), P3.1 (hot-reload for rebinding persistence)**

The raw input layer provides `IsKeyPressed(KeyCode)` / `IsMouseButtonPressed(MouseCode)` — correct and type-safe, but game code must hard-wire physical inputs to gameplay intent. The next step is an abstraction that maps physical inputs to semantic game actions, configured in data.

#### Design Sketch

- TOML-defined action map loaded at startup:
  ```toml
  [actions.move_forward]
  keys = ["W"]
  gamepad = ["LeftStickUp"]

  [actions.jump]
  keys = ["Space"]
  gamepad = ["South"]

  [actions.fire]
  mouse = ["ButtonLeft"]
  gamepad = ["RightTrigger"]
  ```
- `InputActionMap` class: loads TOML, maps raw `KeyCode`/`MouseCode`/gamepad inputs to named actions.
- Query API: `inputActions.IsActionPressed("jump")`, `inputActions.GetAxis("move_horizontal")`.
- Supports rebinding at runtime (write back to TOML).
- Composable: multiple maps can be layered (gameplay map + UI map + debug map) with priority.

#### Definition of Done

- `InputActionMap` loads from TOML and resolves named actions.
- Game code queries actions by name, not raw keys.
- Runtime rebinding works and persists to TOML.
- Multiple action maps can be layered with priority.

---

## Execution Order (Recommended)

```
Phase 1: Foundation
├── P1.1  Test coverage         ←── START HERE
├── P1.2  Generational handles   (can parallel with P1.1)
└── P1.3  InternedString IDs    (can parallel with P1.1)

Phase 2: Architecture  
├── P1.4  Break up Renderer     (after P1.1 for safety net)
├── P1.5  Application decomp    (after or with P1.4)
├── P2.5  Blend state           (needed before transparent rendering)
└── P2.6  RenderPipeline        (during P1.4)

Phase 3: Data & Systems
├── P2.1  Scene entity index
├── P2.2  MeshComponent split
├── P2.3  Frame allocator       
├── P2.4  Physics subsystem     (validates architecture)
├── P2.7  Error handling        (incremental, alongside everything)
└── P2.8  Event queue           (decouple dispatch from polling)

Phase 4: Polish
├── P3.1  Hot-reload
├── P3.2  Explicit CMake lists
├── P3.3  clang-tidy
├── P3.4  Prefab instantiation
├── P3.5  RenderMeshHandle
├── P3.6  GPU debug annotations
├── P3.7  Multiple render targets (MRT)
├── P3.8  Upload batching       (when profiling shows need)
└── P3.9  Sub-sort key          (when ordering artifacts appear)

Phase 5: Horizon
├── P4.1  Mesh assets
├── P4.2  Composable vertex attributes
├── P4.3  Texture asset pipeline
├── P4.4  Debug tooling (ImGui)
├── P4.5  Audio
├── P4.6  Scripting
├── P4.7  Editor bootstrap
├── P4.8  CI pipeline
└── P4.9  Input action mapping  (data-driven input abstraction)
```

Items within the same phase can generally be done in parallel. Phase boundaries represent recommended checkpoints — verify everything builds and passes before crossing.

---

## Appendix: Current State Snapshot

For reference, this plan was written against the following engine state:

- **Branch**: `cleaning-up-game`
- **Files**: ~120 source files in `engine/wayfinder/src/`
- **Tests**: 3 files (`RenderPipelineTests`, `RenderGraphTests`, `RenderFeatureTests`)
- **Dependencies**: SDL3 3.2.30, GLM 1.0.1, spdlog 1.15.2, toml++ 3.4.0, nlohmann/json 3.12.0, Tracy 0.11.1, Box2D 3.1.0, Jolt 5.5.0, Flecs 4.1.5, Dear ImGui 1.91.9b, doctest 2.4.11
- **GPU handles**: All `void*`
- **RenderLayerId/RenderPassId**: `std::string`
- **Scene::GetEntityById**: O(n) scan
- **Subsystems**: GameplayTagRegistry, GameStateMachine (2 total)
- **RenderFeatures**: 0 concrete implementations
- **Empty directories**: `ai/`, `animation/`, `audio/`, `debug/`, `physics/`, `scripting/`, `ui/`
