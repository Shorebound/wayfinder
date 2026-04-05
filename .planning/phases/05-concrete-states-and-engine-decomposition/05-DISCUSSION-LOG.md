# Phase 5: Concrete States and Engine Decomposition - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md -- this log preserves the alternatives considered.

**Date:** 2026-04-05
**Phase:** 05-concrete-states-and-engine-decomposition
**Areas discussed:** GameplayState design, EngineRuntime decomposition, Canvas render submission, FpsOverlay migration

---

## GameplayState Design

### What moves from Game into GameplayState?

| Option | Description | Selected |
|--------|-------------|----------|
| Minimal: Simulation only | GameplayState creates Simulation in OnEnter, delegates Update/Render. TagRegistry, GameStateMachine, SubsystemCollection stay separate. | ✓ |
| Full migration: move everything | Move TagRegistry, GameStateMachine, SubsystemCollection, boot scene loading all into GameplayState. | |
| Staged: Simulation + boot scene | Move Simulation and boot scene loading. Leave tag/state machine wiring for Phase 6. | |

**User's choice:** Minimal: Simulation only
**Notes:** Keep GameplayState thin, focused on Simulation lifecycle. Other Game concerns addressed in Phase 6.

### Boot scene loading location

| Option | Description | Selected |
|--------|-------------|----------|
| GameplayState::OnEnter | State loads boot scene when it enters. Clean per-state ownership. | |
| Simulation::Initialise | Simulation loads boot scene as part of its init. | |
| External: plugin hook | OnStateEnter<GameplayState> hook triggers scene load. | |

**User's choice:** Free text -- user felt the boot scene path should be modular, tied to simulation config. Led to follow-up on ConfigService approach.

### Boot scene config delivery

| Option | Description | Selected |
|--------|-------------|----------|
| SimulationConfig struct | GameplayState populates from ProjectDescriptor, passes to Simulation. | |
| ConfigService-backed (TOML) | Simulation reads from ConfigService (config/simulation.toml). Fully data-driven. | ✓ |
| Direct string parameter | Plain string arg to Simulation::Initialise. | |

**User's choice:** ConfigService-backed. Consistent with Phase 3 architecture -- gameplay/simulation config through the same plugin config system as everything else.
**Notes:** User emphasized using the same architecture everywhere. Gameplay/simulation is likely a plugin, so config flows through builder.LoadConfig.

### EditorState stub scope

| Option | Description | Selected |
|--------|-------------|----------|
| Empty shell: enter/exit only | Proves IApplicationState lifecycle. No simulation, no subsystems. Just logging. | |
| With placeholder UI | Empty shell plus trivial IStateUI showing "Editor" text. | |
| With docking skeleton | Placeholder with ImGui docking setup. More useful for future work. | ✓ |

**User's choice:** With docking skeleton
**Notes:** User wants a foundation for future editor work, not just a minimal proof.

### ECS singleton wiring

| Option | Description | Selected |
|--------|-------------|----------|
| Simulation handles it | Simulation sets up ECS singletons in Initialise() as it owns the world. | ✓ |
| GameplayState delegates to Simulation | GameplayState explicitly calls Simulation methods for wiring. | |
| Separate state subsystem | Dedicated EcsSetupSubsystem. Over-engineered for 3 singletons. | |

**User's choice:** Simulation handles it
**Notes:** Clean ownership -- Simulation owns the world, Simulation sets up its singletons.

---

## EngineRuntime Decomposition

### Subsystem split

| Option | Description | Selected |
|--------|-------------|----------|
| Four subsystems (Window, Input, Time, Renderer) | Each an AppSubsystem. Renderer owns RenderDevice internally. | |
| Five subsystems (split RenderDevice) | RenderDevice as separate subsystem. Future subsystems can depend on device without full renderer. | ✓ |
| Three subsystems (merge Window+Input) | Fewer subsystems but less granular deps. | |

**User's choice:** Five subsystems (split RenderDevice)
**Notes:** User asked for detailed analysis of RenderDevice split. After seeing the dependency graph (AssetService wanting GPU access without pulling in full Renderer, future compute subsystems), chose 5. Discussion covered how 2D/3D/UI rendering are separate RenderFeatures within one Renderer, not separate subsystems.

### Render stack ownership

| Option | Description | Selected |
|--------|-------------|----------|
| Renderer owns everything render | RenderDevice + Renderer + SceneRenderExtractor + BlendableEffectRegistry bundled. | |
| RenderDevice separate | RenderDevice as own subsystem. Renderer + extractor + registry in RendererSubsystem. | ✓ |
| Maximum split | Three separate subsystems for each render component. | |

**User's choice:** RenderDevice separate (tied to previous question)
**Notes:** BlendableEffectRegistry discussed separately -- user asked if it should be a per-subsystem pattern. Concluded it's rendering-specific, belongs in RendererSubsystem, global singleton pattern eliminated.

### Headless support

| Option | Description | Selected |
|--------|-------------|----------|
| Capability-gated (existing system) | Window/Renderer require caps. Headless mode doesn't have those caps -- subsystems don't activate. | ✓ |
| Null backend subsystems | Null implementations that do nothing but are always activated. | |
| Plugin composition (don't register) | Don't register Window/Renderer plugins in headless mode. | |

**User's choice:** Capability-gated
**Notes:** Uses existing Phase 2 capability-gated activation. Clean, consistent.

### SceneRenderExtractor scope

| Option | Description | Selected |
|--------|-------------|----------|
| State-scoped (StateSubsystem) | Per-state extractor, depends on Simulation for Scene access. | |
| App-scoped (inside Renderer) | Keep in Renderer. GameplayState passes Scene each frame. | |
| Dissolve into canvas model | Remove extractor, replace with canvas submission. | |

**User's choice:** Free text -- user wanted real-world examples and comparison with Bevy/Unreal/Oxylus.

Extended discussion followed covering:
1. Renderer must NOT depend on Scene/ECS (abstraction boundary)
2. Canvas model solves this -- canvases contain only render vocabulary
3. Extraction logic (ECS -> render primitives) lives in gameplay domain
4. SceneRenderExtractor is a gameplay-domain utility with cached flecs queries, NOT a flecs system
5. Explicit calling from GameplayState::OnRender, not integrated into flecs pipeline
6. User asked about flecs phases (PreRender system) -- explained timing issues and flexibility advantages of explicit model
7. User confirmed explicit extraction model after understanding performance characteristics

**Final verdict:** Game-side extraction, render-pure canvases, SceneRenderExtractor as gameplay utility class.

---

## Canvas Render Submission

### Canvas types

| Option | Description | Selected |
|--------|-------------|----------|
| Three canvases (Scene/UI/Debug) | Typed per domain. Each has different data shapes. | ✓ |
| Single canvas with channels | Unified with typed channels internally. | |
| Per-phase canvases | One per RenderPhase. Too granular. | |

**User's choice:** Three canvases (after detailed explanation of rationale)
**Notes:** User wanted full understanding of WHY these options exist and what ideal end-state looks like. Detailed analysis provided covering Bevy/Unreal parallels. Three canvases won on type safety and domain clarity.

### Canvas lifecycle

| Option | Description | Selected |
|--------|-------------|----------|
| Renderer-owned, per-frame reset | Buffer reuse, zero alloc after warmup. | ✓ |
| Caller-owned, submitted to Renderer | Per-frame allocation, clear ownership. | |
| Persistent with dirty tracking | Complex, marginal gain. | |

**User's choice:** Renderer-owned (after detailed explanation)
**Notes:** User asked about performance characteristics and future flexibility. Detailed breakdown: per-frame reset with buffer reuse is zero-alloc; persistence upgrade is behind-the-interface swap; real bottleneck is culling/GPU submission, not canvas filling.

### Canvas-to-render-graph flow

| Option | Description | Selected |
|--------|-------------|----------|
| Canvas -> RenderGraph pass mapping | Each canvas feeds its domain of render features. | ✓ |
| Merge into single RenderFrame | Canvases as typed inputs to flat format. | |
| Canvases as render graph resources | Features read directly from canvases. | |

**User's choice:** Canvas -> RenderGraph pass mapping (after full pipeline walkthrough)
**Notes:** User wanted the entire pipeline explained end-to-end. Full frame sequence provided showing BeginFrame -> fill canvases -> process into render graph -> execute -> present. User confirmed after understanding multi-viewport and persistence upgrade paths.

### Performance and future-proofing

User specifically asked about:
1. **Persistence swap**: Confirmed -- Clear()/Submit() interface allows internal swap without consumer changes.
2. **Editor + PIE**: Confirmed -- vector<SceneCanvas> handles N viewports. BackgroundMode negotiation handles editor/game coexistence.
3. **Performance at 50K entities**: Detailed breakdown. Extraction ~1-2ms, canvas fill negligible. Real bottleneck is culling + GPU submission. Canvas model supports all optimizations cleanly.

---

## FpsOverlay Migration

### Data source

| Option | Description | Selected |
|--------|-------------|----------|
| Query TimeSubsystem | Single source of truth, less duplication. | |
| Self-contained rolling average | Current approach, self-contained. | |
| Hybrid: Time delta + own averaging | Time provides delta, overlay does display-optimised averaging. | ✓ |

**User's choice:** Hybrid (after discussion of benefits)
**Notes:** User asked about benefits of relying vs not relying on Time. Hybrid won: Time provides accurate delta (already available), overlay tunes its own smoothing for readability.

### Output target

| Option | Description | Selected |
|--------|-------------|----------|
| UICanvas text overlay | ImGui rendering through canvas system. | ✓ |
| Window title (keep current) | Simple, no rendering dependency. | |
| Both window title and overlay | | |

**User's choice:** UICanvas (via ImGui)
**Notes:** User asked if this should be the first ImGui consumer. Yes -- establishes the pattern for all future ImGui overlays. ImGui draw lists captured into UICanvas, consumed by UI render feature.

### Capability requirements

| Option | Description | Selected |
|--------|-------------|----------|
| Presentation (always-on with window) | Active whenever a window exists. | ✓ |
| No requirements (universal) | Runs in headless too (log output). | |
| Rendering (only with renderer) | Only when renderer is running. | |

**User's choice:** Presentation

### Shipping build presence

| Option | Description | Selected |
|--------|-------------|----------|
| Debug plugin only (not in Shipping) | Not registered in Shipping. | |
| Registered but disabled in Shipping | Available for developer consoles to toggle. | ✓ |
| Compile-time excluded | Zero overhead via #ifdef. | |

**User's choice:** Registered but disabled in Shipping
**Notes:** User asked about evolution to general performance overlay. Decided to name it PerformanceOverlay from the start. Starts minimal (FPS + frame time), structure supports growth. Runtime toggle via ActivateOverlay/DeactivateOverlay.

---

## Agent's Discretion

- FrameCanvases struct layout and accessor design
- SceneCanvas internal data structures
- UICanvas ImGui integration implementation
- DebugCanvas primitives
- SimulationConfig fields beyond boot scene
- EditorState docking layout specifics
- BlendableEffectRegistry internal access pattern
- RenderDeviceSubsystem API surface
- WindowSubsystem wrapping strategy
- InputSubsystem wrapping strategy
- Render feature adaptation to canvas input

## Deferred Ideas

- Persistent canvas with dirty-tracking
- Multi-viewport implementation
- GPU-driven rendering / indirect draw
- Full editor implementation
- PerformanceOverlay evolution to full metrics
- Flecs-integrated extraction (PreRender phase system)
