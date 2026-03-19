# Future Features

Deferred feature ideas from the Core Engine Modernization pass. These are tracked here
so they aren't lost — each is a natural extension of the work already done.

---

## Data-Driven Input Action Mapping

**Context:** With SDL3-native key/mouse codes in place (Phase 3 of core modernization),
the raw input layer is correct and type-safe. The next step is an abstraction that maps
physical inputs to semantic game actions.

**Design sketch:**
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
- `InputActionMap` class: loads TOML, maps raw `KeyCode`/`MouseCode`/gamepad inputs to named actions
- Query API: `inputActions.IsActionPressed("jump")`, `inputActions.GetAxis("move_horizontal")`
- Supports rebinding at runtime (write back to TOML)
- Composable: multiple maps can be layered (gameplay map + UI map + debug map) with priority

**Prerequisites:** Phase 3 (SDL3 input codes), gamepad support in SDL3Input

---

## Hot-Reload Configuration

**Context:** `engine.toml` is loaded once at startup (Phase 1). For editor/development
workflows, live-reloading config without restarting would be valuable.

**Design sketch:**
- File watcher on `engine.toml` (and potentially other TOML configs like input maps, material defs)
- On change: re-parse, diff against current config, apply deltas
- Some settings are hot-swappable (window title, vsync, log level)
- Some require restart (backend selection, screen resolution) — log a warning instead
- Could use platform file-watch APIs (ReadDirectoryChangesW on Windows, inotify on Linux)
  or a polling approach (simpler: check mtime every N seconds)

**Prerequisites:** Phase 1 (EngineConfig), editor infrastructure

---

## Event Queue (Deferred Dispatch)

**Context:** The event system currently uses synchronous blocking dispatch. The comment
in Event.h itself notes this as a known limitation.

**Design sketch:**
- `EventQueue` class: `std::vector<std::unique_ptr<Event>>`
- `Push(event)` during SDL polling, `Drain()` at a defined frame point
- Application decides dispatch timing: immediate for latency-sensitive (window close),
  queued for input events that can batch
- Useful for deterministic replay / networking later

**Prerequisites:** Phase 4 (event system connected to SDL3)

---

## Upload Batching for TransientBufferAllocator

**Context:** Each `UploadToBuffer` call currently creates a staging transfer buffer,
acquires a dedicated command buffer, begins a copy pass, uploads, ends the pass,
submits, and releases the transfer buffer. This is O(N) command buffer submissions
for N transient allocations per frame. With 1-2 allocations per frame (debug lines,
grid) this is fine, but it will become a bottleneck with particles, text quads, or
many dynamic geometry sources.

**Design sketch:**
- Queue all staging writes during the frame into a single large staging buffer
- At a defined flush point (after all Allocate calls, before rendering), submit
  one command buffer with one copy pass containing all uploads
- Alternatively: use persistent buffer mapping (if SDL_GPU exposes it) to skip
  staging entirely for transient data

**When:** When profiling shows upload overhead matters (Stage 7+, particles, lots of dynamic geometry)

**Prerequisites:** TransientBufferAllocator (Stage 4.5), profiling infrastructure

---

## Dynamic Vertex Layout Construction

**Context:** `VertexLayout` stores a raw pointer to a `constexpr` global `VertexAttrib`
array. This is safe because the layouts are `inline constexpr` globals with program
lifetime. If layouts ever need to be built dynamically (e.g., runtime vertex format
selection, mesh import with variable attributes), the pointer-to-global pattern breaks.

**Design sketch:**
- Replace raw pointer + count with a small fixed array (`std::array<VertexAttrib, 8>`)
  or a `SmallVector` inside `VertexLayout`
- Keeps value semantics — safe to copy, store, pass around

**When:** When mesh loading or runtime vertex format selection is needed

**Prerequisites:** Mesh loading (Stage 5+)

---

## Sub-Sort Key Utilization

**Context:** The 64-bit sort key reserves 14 bits for sub-sort (tiebreaker), but
currently only uses `SortPriority` (a `uint8_t`, 0-255). The extra 6 bits of headroom
are unused. Potential future uses: entity ID hash for deterministic ordering across
frames, sequence counter for submission-order stability.

**When:** When non-deterministic draw ordering causes visible artifacts (shimmer, Z-fighting)

**Prerequisites:** Sort-key system (Stage 4.5)
