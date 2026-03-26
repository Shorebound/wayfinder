# Plan: Render Graph Buffer Resource Support (#99)

## TL;DR
The render graph only tracks textures. Add first-class buffer resources (storage buffers) so compute passes can declare buffer read/write dependencies, enabling GPU-driven rendering, compute culling, particles, and structured buffer effects. Extends the unified graph handle to cover both textures and buffers with identical dependency tracking.

## Design Decisions

### Auto-bind vs Manual bind for compute passes

**Chosen: Auto-bind via graph** — the graph collects declared buffer writes and passes them to `BeginComputePass()` automatically; read-only buffers are resolved via `RenderGraphResources::GetBuffer()` and bound by the execute callback.

**Why auto-bind:**
- SDL_GPU requires RW storage buffers at `SDL_BeginGPUComputePass()` time. If the graph doesn't know about buffer writes, it can't set up the compute pass correctly. The callback would need to manage pass creation itself, defeating the graph's purpose.
- Dependency tracking and dead-pass culling can't work for buffer-only passes if the graph doesn't know about buffer access. A compute pass that only writes buffers (no texture output) would always be culled.
- Matches how textures already work: graph auto-manages render targets (write side); callbacks sample textures (read side) using resolved handles.
- Consistent with industry practice (Unreal RDG, Frostbite Frame Graph): resources declared through the builder are auto-bound by the graph.
- Satisfies the engine principle: "explicit over implicit — capabilities are checked, passes are validated, nothing silently dropped."

**Auto-bind cons:**
- `BeginComputePass()` signature changes on `RenderDevice` (breaking change — acceptable for greenfield engine).
- Execute() becomes slightly more complex: must resolve buffer handles and build SDL binding arrays before calling `BeginComputePass`.
- Read-only vs read-write distinction must be tracked per resource, since SDL_GPU binds them at different points.

**Manual bind cons (why we rejected it):**
- Breaks the fundamental render graph contract. Passes could access buffers the graph doesn't know about → incorrect dependency ordering, incorrect culling.
- Graph can't insert barriers for unknown resources (relevant for future Vulkan/D3D12 backends).
- Inconsistent API: textures are graph-managed, buffers would be manually managed.

### ResourceEntry: `std::variant<TextureResource, BufferResource>`
- Type-safe, idiomatic C++23, eliminates invalid state combinations.
- Visitor pattern for Execute()'s resource allocation/binding.
- Shared fields (Name, IsTransient, WrittenByPass, LastReadByPass) remain on the outer ResourceEntry.

### BufferUsage: Full bitmask
- Converts from simple enum to `uint32_t` flags, matching `TextureUsage` style.
- Includes all storage flags now (GraphicsStorageRead, ComputeStorageRead, ComputeStorageWrite) plus Indirect for future indirect draw.
- Existing Vertex/Index become flag values.

---

## Industry Comparison

| Engine | Buffer in Graph | Binding Model | Dependency | Pooling |
|--------|---------------|---------------|------------|---------|
| **Unreal RDG** | `FRDGBuffer` — first class | Auto-bind via pass parameter structs (SRV/UAV) | Automatic barriers from declared access | Transient aliasing across non-overlapping lifetimes |
| **Frostbite** | Unified handles (texture or buffer) | Builder declares reads/writes; graph auto-binds | Topological sort + automatic barriers | Resource aliasing from frame allocator |
| **Bevy** | `SlotType::Buffer` — typed graph slots | Nodes bind via wgpu bind groups (semi-manual) | Slot connection defines ordering | wgpu manages internally |
| **Spartan** | `RHI_Buffer` — state-tracked | Resource state transitions (UAV↔SRV) | Auto barriers from state changes | Per-frame transient pool |
| **bgfx** | No formal graph; `setBuffer()` per stage | Manual binding with implicit sequencing | Internal auto-sequencing from resource use | Transient buffers via `bgfx::alloc()` |
| **Godot 4** | `storage_buffer_create()` in RenderingDevice | Manual; graph tracks for barriers | Automatic barrier insertion | No pooling; create/destroy per use |
| **Wayfinder (proposed)** | `std::variant<Texture, Buffer>` in `ResourceEntry` | Auto-bind RW at pass-begin; RO via resources | Same dependency tracking as textures (WrittenByPass/DependsOn) | `TransientBufferPool` keyed by (size, usage) |

**Key takeaway**: All modern engines with render graphs treat buffers as first-class resources in the graph. The proposed design aligns most closely with Frostbite's unified-handle model and Unreal's auto-binding approach.

---

## Steps

### Phase 1: Backend Extensions
*No dependencies. All changes are additive to RenderDevice.*

**1.1 — Extend `BufferUsage` to a bitmask (parallel with 1.2)**
- File: `engine/wayfinder/src/rendering/backend/RenderDevice.h`
- Convert `BufferUsage` from `enum class : uint8_t { Vertex, Index }` to `enum class : uint32_t` with flag values.
- Add bitwise operators (matching existing `TextureUsage` pattern in `RenderTypes.h`).
- Values: `Vertex = 1<<0`, `Index = 1<<1`, `GraphicsStorageRead = 1<<2`, `ComputeStorageRead = 1<<3`, `ComputeStorageWrite = 1<<4`, `Indirect = 1<<5`.
- Update `SDLGPUDevice::CreateBuffer` to map flags to `SDL_GPUBufferUsageFlags` (combine `SDL_GPU_BUFFERUSAGE_*` flags). Current code uses a ternary on `BufferUsage::Vertex`/`Index` — replace with flag mapping.
- Update `BufferCreateDesc.usage` default to `BufferUsage::Vertex` (no change to existing callers).

**1.2 — Add `ComputePassDescriptor` and storage buffer binding methods (parallel with 1.1)**
- File: `engine/wayfinder/src/rendering/backend/RenderDevice.h`
- Add `ComputePassDescriptor` struct: `std::span<const GPUBufferHandle> ReadWriteStorageBuffers`, `const char* DebugName`.
  - (Skip ReadWriteStorageTextures for now — out of scope for #99. Can add when needed.)
- Change `BeginComputePass()` to `BeginComputePass(const ComputePassDescriptor& desc = {})`.
- Add `virtual void BindComputeStorageBuffers(uint32_t firstSlot, std::span<const GPUBufferHandle> buffers) = 0;` for read-only buffers bound after pipeline.
- File: `engine/wayfinder/src/rendering/backend/sdl_gpu/SDLGPUDevice.h/.cpp`
  - `BeginComputePass`: Build `SDL_GPUStorageBufferReadWriteBinding[]` from desc, pass to `SDL_BeginGPUComputePass()`.
  - `BindComputeStorageBuffers`: Resolve handles → `SDL_GPUBuffer*`, call `SDL_BindGPUComputeStorageBuffers()`.
- File: `engine/wayfinder/src/rendering/backend/null/NullDevice.h`
  - Update `BeginComputePass` signature. No-op implementations.
  - Add `BindComputeStorageBuffers` no-op.
  - **Fix `CreateBuffer`** to return valid distinguishable handles (`{.Index = m_nextId++, .Generation = 1}`) — currently returns `{}` (invalid), which would break buffer resource tests.

**1.3 — Optionally add graphics-side storage binding methods**
- `BindVertexStorageBuffers()`, `BindFragmentStorageBuffers()` — wrapping `SDL_BindGPUVertexStorageBuffers()`/`SDL_BindGPUFragmentStorageBuffers()`.
- These aren't strictly needed for #99 (compute-focused) but are tiny additions that complete the storage buffer API. The draw_model_architecture plan already identifies them. Include if convenient, defer if it bloats scope.

### Phase 2: Render Graph Resource Model
*Depends on Phase 1 (BufferUsage flags needed for buffer desc).*

**2.1 — Add `RenderGraphBufferDesc`**
- File: `engine/wayfinder/src/rendering/graph/RenderGraph.h`
- ```
  struct RenderGraphBufferDesc {
      uint32_t SizeInBytes = 0;
      BufferUsage Usage = BufferUsage::ComputeStorageRead;
      const char* DebugName = "";
  };
  ```

**2.2 — Refactor `ResourceEntry` to `std::variant`**
- File: `engine/wayfinder/src/rendering/graph/RenderGraph.h`
- Extract texture-specific fields into `TextureResourceData`:
  ```
  struct TextureResourceData {
      RenderGraphTextureDesc Desc;
      bool IsReadAsSampler = false;
  };
  ```
- Add `BufferResourceData`:
  ```
  struct BufferResourceData {
      RenderGraphBufferDesc Desc;
  };
  ```
- `ResourceEntry` becomes:
  ```
  struct ResourceEntry {
      std::variant<TextureResourceData, BufferResourceData> Data;
      InternedString Name;
      bool IsTransient = true;
      uint32_t WrittenByPass = UINT32_MAX;
      uint32_t LastReadByPass = UINT32_MAX;
  };
  ```
- Update all existing code that accesses `ResourceEntry.Desc` or `ResourceEntry.IsReadAsSampler` to use `std::get<TextureResourceData>` or `std::visit`.

**2.3 — Add `TransientBufferPool`**
- New file: `engine/wayfinder/src/rendering/resources/TransientBufferPool.h`
- New file: `engine/wayfinder/src/rendering/resources/TransientBufferPool.cpp`
- Mirrors `TransientResourcePool` design. Keyed by `{uint32_t SizeInBytes, BufferUsage Usage}`.
- API: `Initialise(RenderDevice&)`, `Shutdown()`, `Acquire(BufferCreateDesc)`, `Release(GPUBufferHandle, BufferCreateDesc)`.
- For initial implementation: match by exact size. Future: largest-fits or bucket sizes.
- Note: Buffer pooling is simpler than texture pooling — no format/dimensions matrix, just size+usage. Exact-match is reasonable for compute buffers where sizes are typically fixed per frame.
- Add files to `engine/wayfinder/CMakeLists.txt`.

**2.4 — Extend `RenderGraphResources`**
- File: `engine/wayfinder/src/rendering/graph/RenderGraph.h`
- Add `GPUBufferHandle GetBuffer(RenderGraphHandle handle) const;`
- Add `std::vector<GPUBufferHandle> m_buffers;` alongside existing `m_textures`.
- Handles index into the unified `m_resources` list — the resolved handle goes to the appropriate vector based on resource type.

### Phase 3: Builder API & Dependency Tracking
*Depends on Phase 2.*

**3.1 — Extend `RenderGraphBuilder`**
- File: `engine/wayfinder/src/rendering/graph/RenderGraph.h` and `.cpp`
- Add `RenderGraphHandle CreateTransientBuffer(const RenderGraphBufferDesc& desc);`
  - Allocates a `ResourceEntry` with `BufferResourceData`.
- Add `void ReadBuffer(RenderGraphHandle handle);`
  - Records dependency on writer pass (identical logic to `ReadTexture`, minus `IsReadAsSampler`).
  - Adds handle to `pass.Reads`.
- Add `void WriteBuffer(RenderGraphHandle handle);`
  - Records `WrittenByPass = m_passIndex`.
  - Adds handle to new `pass.BufferWrites` vector.
  - If buffer was previously written by another pass, adds dependency (like `WriteColour` with `LoadOp::Load` — a write-after-write means the new writer depends on the old writer for ordering).

**3.2 — Extend `PassEntry`**
- File: `engine/wayfinder/src/rendering/graph/RenderGraph.h`
- Add `std::vector<RenderGraphHandle> BufferWrites;` — RW storage buffer handles for this compute pass.
  - These are passed to `BeginComputePass()` at execute time.

**3.3 — Add `ImportBuffer` and `FindHandle` support**
- `ImportBuffer(name)` creates an external buffer resource (like `ImportTexture`).
- `FindHandle` already works name-based — buffer resources found the same way as textures. No change needed for FindHandle itself.

**3.4 — Verify dependency tracking**
- `ReadBuffer` → dependency on `WrittenByPass` (same as `ReadTexture`)
- `WriteBuffer` → sets `WrittenByPass`, dependency on previous writer if exists
- Dead-pass culling backward propagation already works on `DependsOn` — buffer writes keep producers alive through the same mechanism.
- **Important**: A compute pass that only writes buffers (no swapchain output) is only kept alive if a downstream alive pass reads those buffers. This is correct — orphan buffer writers should be culled.

### Phase 4: Execute Integration
*Depends on Phases 1–3.*

**4.1 — Allocate transient buffers in Execute()**
- File: `engine/wayfinder/src/rendering/graph/RenderGraph.cpp`
- In `Execute()`, alongside transient texture allocation, add a loop for buffer resources:
  - `std::visit` on each ResourceEntry — if `BufferResourceData`, `Acquire` from `TransientBufferPool`.
  - Store result in `resources.m_buffers[i]`.
- Accept `TransientBufferPool&` as a new parameter to `Execute()` (or combine both pools into a single `TransientResourcePools` struct).

**4.2 — Build compute pass descriptor from declared writes**
- In Execute()'s compute pass branch, before calling `BeginComputePass`:
  - Collect `GPUBufferHandle`s from `pass.BufferWrites` → resolved from `resources.m_buffers`.
  - Build `ComputePassDescriptor` with these handles.
  - Call `device.BeginComputePass(desc)`.

**4.3 — Release transient buffers**
- After all passes execute, release buffer handles back to `TransientBufferPool` (same pattern as texture release).

**4.4 — Update call sites**
- `RenderGraph::Execute()` signature changes to accept both pools. Update `Renderer::Render()` where `Execute` is called.
- `RenderContext` should own the `TransientBufferPool` alongside `TransientResourcePool`.

### Phase 5: Tests
*Depends on Phase 4.*

**5.1 — Compute pass writes buffer, raster pass reads it** (required by Definition of Done)
- File: `tests/rendering/RenderGraphTests.cpp`
- Compute pass: `CreateTransientBuffer(desc)` + `WriteBuffer(handle)`.
- Raster pass: `ReadBuffer(handle)` + `SetSwapchainOutput()`.
- Verify execution order: compute first, then raster.

**5.2 — Buffer-only dead pass culling**
- Compute pass writes a buffer that nobody reads → should be culled.
- Compute pass writes a buffer → another pass reads it → writes swapchain → both alive.

**5.3 — Mixed texture + buffer dependencies**
- Pass A writes a texture. Pass B reads the texture, writes a buffer. Pass C reads the buffer, writes swapchain.
- Verify all three execute in order A → B → C.

**5.4 — Transient buffer pool roundtrip**
- Acquire → Release → Acquire with same desc returns valid handle.

**5.5 — Existing tests pass unchanged**
- All existing `RenderGraphTests.cpp` cases must continue to pass.

---

## Relevant Files

- `engine/wayfinder/src/rendering/graph/RenderGraph.h` — Core changes: `RenderGraphBufferDesc`, variant `ResourceEntry`, `RenderGraphBuilder` buffer methods, `PassEntry::BufferWrites`, `RenderGraphResources::GetBuffer`
- `engine/wayfinder/src/rendering/graph/RenderGraph.cpp` — Builder buffer methods, Execute buffer allocation/binding/release
- `engine/wayfinder/src/rendering/backend/RenderDevice.h` — `BufferUsage` flags, `ComputePassDescriptor`, `BeginComputePass` signature, `BindComputeStorageBuffers`
- `engine/wayfinder/src/rendering/backend/sdl_gpu/SDLGPUDevice.h/.cpp` — SDL_GPU implementations of new methods
- `engine/wayfinder/src/rendering/backend/null/NullDevice.h` — No-op implementations, fix `CreateBuffer` handles
- `engine/wayfinder/src/rendering/resources/TransientBufferPool.h/.cpp` — New file: buffer pool
- `engine/wayfinder/src/rendering/resources/TransientResourcePool.h` — Reference for pool design
- `engine/wayfinder/src/rendering/RenderContext.h/.cpp` — Own TransientBufferPool
- `engine/wayfinder/src/rendering/Renderer.h/.cpp` — Pass buffer pool to Execute
- `engine/wayfinder/src/rendering/RenderTypes.h` — Reference for TextureUsage bitmask pattern
- `tests/rendering/RenderGraphTests.cpp` — New buffer resource tests
- `engine/wayfinder/CMakeLists.txt` — Add TransientBufferPool source files
- `tests/CMakeLists.txt` — May need updates if new test files added

## Verification

1. `cmake --build --preset debug` — all targets compile cleanly
2. `ctest --preset test` — all existing tests pass
3. New render graph buffer tests pass: compute→raster dependency chain, buffer dead-pass culling, mixed texture+buffer, transient pool roundtrip
4. Manual: add a dummy compute pass with buffer I/O in Journey sandbox to verify end-to-end with SDL_GPU backend (optional — tests cover correctness)

## Scope Boundaries

**Included:**
- Buffer resources in the render graph (create, read, write, dependency tracking, culling, execution)
- BufferUsage bitmask extension (full set of flags)
- ComputePassDescriptor and storage buffer binding (read-only + read-write)
- TransientBufferPool
- NullDevice fix for buffer handles
- Tests covering all Definition of Done criteria

**Excluded:**
- Graphics-side storage buffer binding (`BindVertexStorageBuffers`, `BindFragmentStorageBuffers`) — small additions but not needed for #99. Can be added as part of the draw model work.
- Indirect draw buffer support — flag is defined but not wired.
- Compute shaders, shader compilation, actual GPU compute workloads — #99 is infrastructure only.
- Buffer aliasing / lifetime optimisation — exact-match pooling first, optimise later.
- Read-write storage textures in compute passes — similar pattern, add when needed.
