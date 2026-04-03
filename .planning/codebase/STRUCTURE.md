# Directory Structure

**Last mapped:** 2026-04-03
**Focus areas:** core, app, plugins, gameplay/simulation

---

## Top-Level Layout

```
wayfinder/
  CMakeLists.txt              # Root build config
  CMakePresets.json            # Build presets (dev, shipping, CI)
  .clang-format               # Code formatting rules
  .clang-tidy                 # Static analysis config
  README.md
  wayfinder_project.slnf      # VS solution filter

  engine/wayfinder/           # Engine library
  sandbox/journey/            # Game sandbox
  sandbox/waystone/           # Runtime launcher (planned)
  apps/cartographer/          # Editor (planned)
  apps/compass/               # Project manager (planned)
  apps/waystone/              # Launcher (planned)
  tests/                      # Test suites
  tools/                      # CLI tools and scripts
  thirdparty/                 # CPM-managed dependencies
  cmake/                      # CMake modules
  docs/                       # Documentation and plans
  bin/                        # Build output
  build/                      # Build intermediates
```

---

## Engine Library (`engine/wayfinder/`)

```
engine/wayfinder/
  CMakeLists.txt              # Engine library target
  shaders/                    # Slang shader source files

  src/
    WayfinderPCH.h            # Precompiled header

    core/                     # Foundation primitives (no engine deps)
      Core.h                  # Master include
      Assert.h                # WAYFINDER_ASSERT macro
      Handle.h                # Generational handles: Handle<TTag>, OpaqueHandle<TTag>
      Identifiers.h           # StringHash (FNV-1a), Uuid, SceneObjectId
      InternedString.h        # Globally interned strings, O(1) equality
      Log.h                   # Category-based logging API
      Profiling.h             # Tracy wrapper macros
      ResourcePool.h          # Handle-indexed pool with free-list
      Result.h                # Result<T> alias over std::expected
      TransparentStringHash.h # Heterogeneous string map lookup
      Types.h                 # GLM aliases (Float2/3/4, Matrix4, Quaternion)

      events/                 # Event system
        Event.h               # EventType enum, base types
        EventQueue.h          # Deferred typed event queue
        TypedEventBuffer.h    # Per-type event storage
        ApplicationEvent.h    # WindowClose, WindowResize
        KeyEvent.h            # KeyPressed, KeyReleased, KeyTyped
        MouseEvent.h          # MouseMoved, MouseButton, MouseScroll

      logging/                # Logging backend
        ILogger.h             # Logger interface
        ILogOutput.h          # Output target interface
        LogTypes.h            # Verbosity, config enums
        spdlog/               # spdlog implementation
          SpdLogger.h
          SpdLogOutput.h
          SpdLogManager.h

    app/                      # Application lifecycle
      Application.h/cpp       # Entry point, frame loop, event routing
      EngineRuntime.h/cpp     # Monolithic service container
      EngineConfig.h/cpp      # TOML config loading
      EngineContext.h          # Non-owning service bundle (v1 - minimal)
      EntryPoint.h             # main() via CreateGamePlugin()
      Layer.h                  # Base layer interface
      LayerStack.h/cpp         # Ordered layer management
      Subsystem.h              # Subsystem base + SubsystemCollection<TBase>
      FpsOverlayLayer.h        # Debug FPS overlay

    plugins/                  # Extension architecture
      Plugin.h                # Plugin base class
      PluginRegistry.h/cpp    # Descriptor store with typed registrars
      PluginLoader.h/cpp      # DLL plugin loading (v1 - removal planned)
      PluginExport.h          # DLL export macros (v1 - removal planned)

      registrars/             # Domain-specific registrars
        SystemRegistrar.h/cpp   # ECS system declarations + topological sort
        StateRegistrar.h/cpp    # Game state declarations + validation
        TagRegistrar.h/cpp      # Gameplay tag declarations (code + TOML)

    gameplay/                 # Simulation root (-> renamed to "simulation" in v2)
      Game.h/cpp              # Game root: flecs world, subsystems, scene
      GameContext.h            # Init context struct
      GameState.h/cpp         # ActiveGameState singleton, RunCondition helpers
      GameStateMachine.h/cpp  # State transitions, conditioned system eval
      GameplayTag.h/cpp       # Hierarchical dot-separated tag type
      GameplayTagRegistry.h/cpp # Tag validation and file loading

    scene/                    # Entity/component/scene management
      Scene.h/cpp             # Entity creation, load/save from JSON
      SceneDocument.h/cpp     # Scene serialisation format (version 1)
      SceneSettings.h         # Scene-level settings
      SceneWorldBootstrap.h/cpp # Headless world setup for tests/tools
      ComponentRegistry.h/cpp  # Compile-time core component registry
      RuntimeComponentRegistry.h/cpp # Unified engine + game component registry
      Components.h/cpp         # Core ECS component types

      entity/                 # Entity wrapper
        Entity.h/cpp          # Typed component access wrapper

      plugins/                # Scene-level ECS plugins
        TransformPlugin.h/cpp   # Transform propagation system
        CameraPlugin.h/cpp      # Active camera extraction system

    ecs/                      # ECS integration
      Flecs.h                 # Flecs include wrapper

    rendering/                # Render pipeline
      backend/                # GPU abstraction
        RenderDevice.h        # Abstract GPU interface
        sdl_gpu/              # SDL_GPU implementation (~1400 lines)
        null/                 # Headless stub
      graph/                  # Render graph
        RenderGraph.h         # DAG-based render pass scheduling
        RenderFrame.h         # Per-frame snapshot data
        RenderPass.h          # Pass interface
        RenderIntent.h        # Material intent routing
      pipeline/               # Render orchestration
        RenderOrchestrator.h  # Feature management + graph building
        RenderFeature.h       # Feature base class
        SceneRenderExtractor.h # ECS -> render data extraction
        DefaultFeatures.h     # @prototype hardcoded feature set
      passes/                 # Concrete render passes
        SceneOpaquePass.h     # Opaque geometry pass
        CompositionPass.h     # Final composition
        DebugPass.h           # Debug visualisation
        PostProcessPass.h     # Post-processing
        SkyPass.h             # Sky rendering
        PresentPass.h         # Swapchain blit
      materials/              # Material system
        Material.h/cpp        # Material loading and properties
        SlangCompiler.h/cpp   # Shader compilation wrapper
        SlangReflection.h/cpp # Shader reflection queries
      mesh/                   # Mesh management
        Mesh.h/cpp            # Mesh loading and GPU upload
        MeshFormat.h          # Binary mesh format
        MeshManager.h         # Handle-based mesh storage
      services/               # Render services
        TextureManager.h      # Texture loading and caching
        FrameAllocator.h      # Per-frame memory allocation

    assets/                   # Asset management
      AssetService.h/cpp      # Central asset loading and caching

    physics/                  # Physics integration
      PhysicsPlugin.h/cpp     # Plugin registration
      PhysicsSubsystem.h/cpp  # GameSubsystem lifecycle
      PhysicsWorld.h/cpp      # Jolt world wrapper
      JoltUtils.h             # Type conversions

    volumes/                  # Post-process volumes
      BlendableEffect.h       # Type-erased blendable effect
      BlendableEffectRegistry.h/cpp # Effect registration and blending

    platform/                 # Platform abstraction
      BackendConfig.h         # Backend selection enums
      Window.h                # Window interface
      Input.h                 # Input interface
      Time.h                  # Time interface
      sdl3/                   # SDL3 implementations
        SDL3Window.h/cpp
        SDL3Input.h/cpp
        SDL3Time.h/cpp
      null/                   # Headless stubs
        NullWindow.h
        NullInput.h
        NullTime.h
```

---

## Sandbox (`sandbox/journey/`)

```
sandbox/journey/
  CMakeLists.txt
  src/
    JourneyGame.cpp           # Root plugin: systems, components, states, tags
```

Single file demonstrating the full plugin API: custom components (HealthComponent), conditioned systems (HealthRegen, BurnDamage), game states, gameplay tags. Uses `WAYFINDER_IMPLEMENT_GAME_PLUGIN(JourneyGame)` macro.

---

## Tests (`tests/`)

```
tests/
  CMakeLists.txt              # 4 test executables
  TestHelpers.h               # FixturesDir(), MakeTestRegistry()

  core/                       # Core primitives tests
    HandleTests.cpp
    InternedStringTests.cpp
    EventQueueTests.cpp
    IdentifierTests.cpp
    ResultTests.cpp
    MeshFormatTests.cpp
    AssetPipelineTests.cpp
    FrustumTests.cpp

  app/                        # Application layer tests
    EngineRuntimeTests.cpp
    EngineConfigTests.cpp
    SubsystemTests.cpp

  gameplay/                   # Gameplay/simulation tests
    GameplayTagTests.cpp
    GameplayTagRegistryTests.cpp

  scene/                      # Scene/ECS tests
    SceneComponentTests.cpp
    ECSIntegrationTests.cpp
    SceneLoadTests.cpp
    SceneSaveTests.cpp
    ComponentSerialisationTests.cpp
    ComponentValidationTests.cpp

  rendering/                  # Rendering pipeline tests
    RenderOrchestratorTests.cpp
    RenderGraphTests.cpp
    RenderFeatureTests.cpp
    SubmissionDrawingTests.cpp
    SceneOpaquePassTests.cpp
    PostProcessFeatureTests.cpp
    DebugPassTests.cpp
    RenderServicesTests.cpp
    BlendStateTests.cpp
    FrameAllocatorTests.cpp
    TextureManagerTests.cpp
    MeshManagerTests.cpp
    SlangCompilerTests.cpp
    SlangReflectionTests.cpp

  volumes/                    # Post-process volume tests
    BlendableEffectRegistryTests.cpp

  physics/                    # Physics tests
    PhysicsTests.cpp
    PhysicsIntegrationTests.cpp

  fixtures/                   # Test data files
    test_scene.json           # Complete scene
    minimal_scene.json        # Minimal valid scene
    hierarchy_scene.json      # Parent-child entities
    bad_scene.json            # Invalid scene (error testing)
    duplicate_scene_object_ids.json
    test_tags.toml            # Tag definitions
    test_engine_config.toml   # Engine config
    textured_material.json    # Material fixtures
    unlit_blended_material.json
    material_bad_texture_id.json
    texture_missing_id.json
    blend_test/               # Blendable effect data
    mesh/                     # Binary mesh data
    shaders/                  # Shader source for tests
    temp/                     # Transient output
```

### Test Executables

| Target | Scope | Deps |
|--------|-------|------|
| `wayfinder_core_tests` | Core, app, gameplay | Engine lib + doctest |
| `wayfinder_render_tests` | Rendering pipeline | Engine lib + doctest + Slang runtime |
| `wayfinder_scene_tests` | Scene/ECS | Engine lib + doctest |
| `wayfinder_physics_tests` | Physics integration | Engine lib + doctest |

---

## Tools (`tools/`)

```
tools/
  lint.py                     # Format checking + banned includes
  tidy.py                     # clang-tidy static analysis
  format-fixup.py             # Domain-specific format fixups
  install-hooks.py            # Git hook installer
  ci-local.py                 # Local CI simulation
  gh-issues.py                # GitHub issue management (blocked-by, sub-issues)

  waypoint/                   # Asset CLI tool
  expedition/                 # (tool)
  navigator/                  # (tool)
  surveyor/                   # (tool)
  shader_manifest/            # Shader manifest tooling
```

---

## Documentation (`docs/`)

```
docs/
  project_vision.md           # Full project rationale
  workspace_guide.md           # Repo navigation guide
  render_passes.md             # Render pass documentation
  user_commands.md             # User-facing command reference
  github_issues.md             # Issue/PR workflow guide

  plans/                       # Architecture plans
    application_architecture_v2.md   # v2 app architecture (authoritative)
    application_migration_v2.md      # v1 -> v2 transition tables
    game_framework.md                # Simulation, states, ServiceProvider
    console.md                       # Debug console plan
    rendering_performance.md         # Hot path goals
```

---

## Naming Conventions

| Concept | Convention | Examples |
|---------|-----------|---------|
| Source files | PascalCase, match primary type | `RenderDevice.h`, `GameStateMachine.cpp` |
| Platform-specific files | Platform suffix | `SDL3Window.cpp`, `NullDevice.cpp` |
| Test files | `<Domain>Tests.cpp` | `HandleTests.cpp`, `SceneLoadTests.cpp` |
| Fixture files | snake_case | `test_scene.json`, `test_tags.toml` |
| Directories | lowercase or PascalCase (component match) | `core/`, `rendering/`, `sdl3/`, `null/` |

---

## File Count Summary

| Domain | Headers | Sources | Total | ~LOC |
|--------|---------|---------|-------|------|
| core | 15 | 1 | 16 | 600 |
| core/events | 7 | 1 | 8 | 200 |
| core/logging | 6 | 1 | 7 | 200 |
| app | 11 | 3 | 14 | 850 |
| plugins | 4 | 1 | 5 | 300 |
| plugins/registrars | 3 | 3 | 6 | 150 |
| gameplay | 7 | 4 | 11 | 400 |
| scene | 5 | 2 | 7 | 300 |
| scene/entity | 1 | 1 | 2 | 70 |
| scene/plugins | 2 | 2 | 4 | 100 |
| ecs | 1 | 0 | 1 | 20 |
| rendering | ~30 | ~20 | ~50 | ~3000 |
| assets | 1 | 1 | 2 | ~150 |
| physics | 4 | 4 | 8 | ~500 |
| volumes | 2 | 1 | 3 | ~200 |
| platform | ~9 | ~6 | ~15 | ~500 |
| **Engine total** | | | **~160** | **~7500** |
| Tests | 0 | ~37 | ~37 | ~3000 |
| Sandbox | 0 | 1 | 1 | ~250 |
