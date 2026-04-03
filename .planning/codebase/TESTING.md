# Testing

**Last mapped:** 2026-04-03
**Focus areas:** core, app, plugins, gameplay/simulation

---

## Framework

- **doctest** 2.4.11 (header-only, BDD-style)
- Custom main: `cmake/doctest_main.cpp` (`#include <doctest/doctest.h>`)
- All tests headless: Null render device + Null window (no GPU, no window)

---

## Test Executables

| Target | Files | Scope | Extra Deps |
|--------|-------|-------|------------|
| `wayfinder_core_tests` | 13 | Core primitives, app layer, gameplay | - |
| `wayfinder_render_tests` | 16 | Rendering pipeline, volumes | Slang runtime DLLs |
| `wayfinder_scene_tests` | 6 | Scene/ECS, serialisation | - |
| `wayfinder_physics_tests` | 2 | Physics integration | - |

Build and run:
```powershell
cmake --build --preset debug --target wayfinder_core_tests
ctest --preset test
```

---

## Test Helpers

`tests/TestHelpers.h`:
- `Helpers::FixturesDir()` - path to `tests/fixtures/`
- `Helpers::MakeTestRegistry()` - `RuntimeComponentRegistry` with core scene entries only (no plugins)

---

## Test Fixtures (`tests/fixtures/`)

| File | Purpose |
|------|---------|
| `test_scene.json` | Complete valid scene (entities + components) |
| `minimal_scene.json` | Minimal valid scene structure |
| `hierarchy_scene.json` | Parent-child entity relationships |
| `bad_scene.json` | Invalid scene (error path testing) |
| `duplicate_scene_object_ids.json` | ID conflict detection |
| `test_tags.toml` | Gameplay tag hierarchy definitions |
| `test_engine_config.toml` | Engine configuration |
| `textured_material.json` | Material fixture |
| `unlit_blended_material.json` | Material fixture |
| `material_bad_texture_id.json` | Invalid material reference |
| `texture_missing_id.json` | Missing asset reference |
| `blend_test/` | Blendable effect test data |
| `mesh/` | Binary mesh data |
| `shaders/` | Slang shader source for compiler tests |
| `temp/` | Transient test output |

---

## Headless Testing Pattern

All tests use Null backends - no window creation, no GPU initialisation:

```cpp
// Null render device tracks operations without executing them
struct TrackingRenderDevice final : public RenderDevice
{
    auto Initialise(Window&) -> Result<void> override { return {}; }
    void BeginFrame() override { /* no-op */ }
    void EndFrame() override { /* no-op */ }
    auto CreateBuffer(...) -> GPUBufferHandle override { return {}; }
    // All GPU methods stubbed
};

// EngineRuntime auto-selects null backends
EngineRuntime runtime;
auto result = runtime.Initialise();  // Null device + null window
```

---

## Test Patterns

### Unit Tests (correctness boundaries)

Behaviour-focused names, single assertion focus:

```cpp
TEST_CASE("Result<int> holds a value on success")
{
    Result<int> r(42);
    CHECK(r.has_value());
    CHECK(r.value() == 42);
}

TEST_CASE("Result<void> is falsy on error")
{
    Result<void> r = MakeError("failed");
    CHECK_FALSE(r);
}
```

### Round-Trip Tests (serialisation fidelity)

```cpp
TEST_CASE("Scene round-trips through save and load")
{
    // Create scene, add entities + components
    // Save to temp file
    // Load into fresh scene
    // Verify all entities and component data match
}
```

### Integration Tests (system contracts)

Full system setup: registry + ECS world + scene:

```cpp
TEST_SUITE("ECS Integration")
{
    TEST_CASE("CreateEntity produces a valid entity")
    {
        flecs::world world;
        auto registry = MakeTestRegistry();
        registry.RegisterComponents(world);
        Scene::RegisterCoreComponents(world);
        Scene scene(world, registry, "TestScene");

        auto entity = scene.CreateEntity("Player");
        CHECK(entity.IsValid());
        CHECK(entity.GetName() == "Player");
    }
}
```

### Plugin Integration Tests

Full plugin registration path through subsystems:

```cpp
struct PhysicsIntegrationFixture
{
    ProjectDescriptor Project{.Name = "PhysicsIntegrationTest"};
    PluginRegistry Registry;
    SubsystemCollection Subsystems;
    flecs::world EcsWorld;

    PhysicsIntegrationFixture() : Registry(Project, Config)
    {
        Registry.AddPlugin<PhysicsPlugin>();
        Subsystems.Initialise();
        Registry.ApplyToWorld(EcsWorld);
    }
};
```

### Error Path Tests

Both success and failure paths tested:

```cpp
TEST_CASE("ValidateTextureAssetDocument rejects missing asset_id")
{
    nlohmann::json doc;  // Missing required field
    auto result = ValidateTextureAssetDocument(doc);
    CHECK_FALSE(result);
}
```

---

## Test Coverage by Domain

### Core (`tests/core/`)

| File | Coverage |
|------|----------|
| `HandleTests.cpp` | Generational handles, stale detection, pool lifecycle |
| `InternedStringTests.cpp` | String interning, pointer stability |
| `EventQueueTests.cpp` | Event queuing, FIFO order, drain cycles |
| `IdentifierTests.cpp` | UUID generation, typed IDs (SceneObjectId, AssetId) |
| `ResultTests.cpp` | Result<T> success/error, value_or, propagation |
| `MeshFormatTests.cpp` | Binary mesh format validation, round-trip |
| `AssetPipelineTests.cpp` | Texture/material validation, asset cache |
| `FrustumTests.cpp` | Frustum culling, bound transforms, sphere tests |

### App (`tests/app/`)

| File | Coverage |
|------|----------|
| `EngineRuntimeTests.cpp` | Runtime init, frame lifecycle with null backends |
| `EngineConfigTests.cpp` | Config loading from TOML |
| `SubsystemTests.cpp` | Subsystem registration, init/shutdown ordering |

### Gameplay (`tests/gameplay/`)

| File | Coverage |
|------|----------|
| `GameplayTagTests.cpp` | Tag hierarchies, containment, container operations |
| `GameplayTagRegistryTests.cpp` | Tag file loading, code registration |

### Scene (`tests/scene/`)

| File | Coverage |
|------|----------|
| `SceneComponentTests.cpp` | Core component types |
| `ECSIntegrationTests.cpp` | Entity creation, component CRUD, transforms |
| `SceneLoadTests.cpp` | JSON scene loading with fixtures |
| `SceneSaveTests.cpp` | Scene serialisation |
| `ComponentSerialisationTests.cpp` | Component round-trip fidelity |
| `ComponentValidationTests.cpp` | Schema validation, error detection |

### Rendering (`tests/rendering/`)

| File | Coverage |
|------|----------|
| `RenderOrchestratorTests.cpp` | Feature management, graph building |
| `RenderGraphTests.cpp` | DAG scheduling, pass ordering |
| `RenderFeatureTests.cpp` | Feature lifecycle, enable/disable |
| `SubmissionDrawingTests.cpp` | Draw call submission |
| `SceneOpaquePassTests.cpp` | Opaque geometry pass |
| `PostProcessFeatureTests.cpp` | Post-process pipeline |
| `DebugPassTests.cpp` | Debug visualisation |
| `RenderServicesTests.cpp` | Service integration |
| `BlendStateTests.cpp` | Blend mode validation |
| `FrameAllocatorTests.cpp` | Per-frame allocation |
| `TextureManagerTests.cpp` | Texture loading/caching |
| `MeshManagerTests.cpp` | Mesh handle management |
| `SlangCompilerTests.cpp` | Shader compilation |
| `SlangReflectionTests.cpp` | Shader reflection queries |

### Physics (`tests/physics/`)

| File | Coverage |
|------|----------|
| `PhysicsTests.cpp` | Physics world operations |
| `PhysicsIntegrationTests.cpp` | Full plugin path, ECS wiring |

---

## Testing Rules (from project conventions)

**Test:** correctness boundaries, round-trip fidelity, contracts between systems, lifecycle/state transitions.

**Skip:** trivial accessors, third-party internals, speculative tests for code that doesn't exist yet.

**Rules:**
- Headless only (no window, no GPU - use Null backend)
- No filesystem outside `tests/fixtures/`
- One file per domain, not per class
- Names describe behaviour, not functions
