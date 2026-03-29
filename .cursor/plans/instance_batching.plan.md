# Plan: Instance Batching for Opaque Geometry

**TL;DR:** Add instanced rendering for opaque scene geometry by grouping consecutive same-mesh+material submissions after the existing sort, allocating per-instance transform data into a GPU storage buffer readable from vertex shaders via `SV_InstanceID`, and issuing `DrawIndexed` with `instanceCount > 1`. Three prerequisite pieces (storage buffer binding, buffer usage flags, BufferUsage::GraphicsStorageRead) then the core batching logic, shader modifications, and test coverage.

**Approach decisions:**
- **Per-instance data:** Storage buffer + `SV_InstanceID` (not instance-rate vertex buffer). More extensible, no pipeline/vertex-layout churn, aligns with draw_model_architecture.md roadmap.
- **Batch detection:** Post-sort scan (not sort key modification). Current sort key groups by material, and identical mesh+material submissions are naturally adjacent within a material group. No depth precision sacrifice.
- **Scope:** Opaque geometry only (SceneOpaquePass). Design notes for transparent included.

---

## Phase A: Prerequisites (RenderDevice + Buffer Infrastructure)

### Step A1 — BufferUsage::GraphicsStorageRead flag
Extend `BufferUsage` from a plain enum to a flags enum so buffers can be created with combined usage (e.g., Vertex | GraphicsStorageRead).

**Changes:**
- `RenderDevice.h` `BufferUsage` — add `GraphicsStorageRead = 0x04` (or similar). Consider bitfield approach.
- `SDLGPUDevice::CreateBuffer()` — map `GraphicsStorageRead` to `SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ` in the usage flags.
- `NullDevice` — no change needed (CreateBuffer is a no-op handle generator).

**Files:**
- `engine/wayfinder/src/rendering/backend/RenderDevice.h` — `BufferUsage` enum, `BufferCreateDesc`
- `engine/wayfinder/src/rendering/backend/sdl_gpu/SDLGPUDevice.cpp` — `CreateBuffer()` usage flag mapping

### Step A2 — RenderDevice::BindVertexStorageBuffers()
Add the ability to bind read-only storage buffers accessible from vertex shaders during a render pass.

**Changes:**
- `RenderDevice.h` — add `virtual void BindVertexStorageBuffers(uint32_t firstSlot, std::span<const GPUBufferHandle> buffers) = 0;`
- `SDLGPUDevice.cpp` — implement via `SDL_BindGPUVertexStorageBuffers(m_renderPass, firstSlot, sdlBindings, count)`. Resolve handles to `SDL_GPUBuffer*`, build `SDL_GPUBuffer*[]`.
- `NullDevice.h` — add empty override.

**Files:**
- `engine/wayfinder/src/rendering/backend/RenderDevice.h`
- `engine/wayfinder/src/rendering/backend/sdl_gpu/SDLGPUDevice.h` + `.cpp`
- `engine/wayfinder/src/rendering/backend/null/NullDevice.h`

### Step A3 — TransientBufferAllocator: Instance Data Ring
Add a third ring buffer for per-frame instance/storage data, or generalise the allocator to support a third allocation category.

**Changes:**
- `TransientBufferAllocator` — add `m_storageRing` / `m_storageCursor` / `m_storageCapacity`. New method: `AllocateStorage(const void* data, uint32_t sizeInBytes) -> TransientAllocation`.
- `Initialise()` — accept `uint32_t storageCapacity` parameter (e.g., 1 MB default — enough for ~16K instances at 64 bytes each).
- `BeginFrame()` — reset `m_storageCursor`.
- The storage ring buffer must be created with `BufferUsage::GraphicsStorageRead` (from Step A1).

**Files:**
- `engine/wayfinder/src/rendering/resources/TransientBufferAllocator.h` + `.cpp`
- Callers of `Initialise()` — pass new capacity parameter (likely `RenderServices` or `RenderContext`)

---

## Phase B: Core Batching Logic

### Step B1 — Batch Detection in RenderOrchestrator::Prepare()
After the existing sort pass in `Prepare()`, add a `BatchSubmissions()` step that walks the sorted submission list and identifies consecutive runs sharing the same {ShaderName, MaterialParameterHash, MeshStableKey}.

**Design:**
1. After `std::ranges::sort(layer.Meshes, ...)`, call `BatchSubmissions(layer)`.
2. Walk submissions linearly. Compare each pair for batch compatibility:
   - Same `submission.Material.ShaderName`
   - Same `submission.Material.Ref` or a material identity hash (parameter content + textures)
   - Same `submission.Mesh.StableKey` + `submission.Mesh.SubmeshIndex`
3. For a run of N compatible submissions (N > 1):
   - Allocate N × sizeof(InstanceData) from the storage ring via `TransientBufferAllocator::AllocateStorage()`.
   - Write each submission's `LocalToWorld` (Matrix4, 64 bytes) into the buffer.
   - Mark the first submission of the run with `InstanceCount = N` and `InstanceDataAllocation = {buffer, offset, size}`.
   - Mark submissions 2..N as `Batched = true` (skip during draw).
4. Submissions with `InstanceCount == 1` (unbatched) still work — they write one transform into the storage buffer and draw with `instanceCount = 1`, OR fall back to the current UBO push path.

**New types:**
```cpp
struct InstanceData {
    Matrix4 Model;      // World transform
    Matrix4 Mvp;        // Pre-computed MVP (avoids per-instance matrix multiply in shader)
};
// 128 bytes per instance
```

Or, to reduce bandwidth and do the multiply in shader:
```cpp
struct InstanceData {
    Matrix4 Model;      // 64 bytes — shader computes MVP = ViewProj * Model
};
```
Recommended: Store only `Model` (64 bytes). Pass View and Proj as a UBO (shared across all instances). Shader computes `MVP = ViewProj * Model` per vertex. This halves instance buffer size and the matrix multiply cost is trivial.

**New fields on RenderMeshSubmission:**
- `uint32_t InstanceCount = 1;`
- `TransientAllocation InstanceData{};` — points to storage buffer region
- `bool Batched = false;` — true for submissions merged into a prior one's batch

**Files:**
- `engine/wayfinder/src/rendering/graph/RenderFrame.h` — add fields to `RenderMeshSubmission`
- `engine/wayfinder/src/rendering/pipeline/RenderOrchestrator.h` — declare `BatchSubmissions()`
- `engine/wayfinder/src/rendering/pipeline/RenderOrchestrator.cpp` — implement `BatchSubmissions()` after sort in `Prepare()`

### Step B2 — Material Identity for Batching
Two submissions can only batch if they have identical material state (same parameters, same textures, same overrides). Need a fast equality check.

**Design:**
- Add `uint64_t ContentHash() const` to `RenderMaterialBinding` that hashes: `ShaderName` + `Parameters` content + `Textures` content + `StateOverrides`. This can be computed once during extraction (in `SceneRenderExtractor`) and cached on the submission.
- Alternative: use the existing 16-bit `MaterialIdBits` from the sort key — submissions in the same material-ID group already share shader + material identity. Verify this is sufficient (it encodes shader name + material ref, but may not capture per-entity overrides).

**Recommendation:** Use `MaterialIdBits` from the sort key as the fast path. If `HasOverrides == true`, don't batch (overrides make equality checking expensive and overridden entities are rare). This avoids needing a new hash.

**Files:**
- `engine/wayfinder/src/rendering/pipeline/RenderOrchestrator.cpp` — batch compatibility check uses sort key material bits + mesh stable key
- No new files needed if using sort key material ID

---

## Phase C: Draw Path Changes

### Step C1 — Instanced Draw Submission
Modify `DrawSubmission()` in SubmissionDrawing.cpp to handle instanced draws.

**Changes:**
1. Skip submissions where `Batched == true` (they're folded into a batch leader).
2. For submissions with `InstanceCount > 1`:
   - Bind the instance storage buffer via `device.BindVertexStorageBuffers(0, {submission.InstanceData.Buffer})`.
   - Push a `ViewProjUBO` (shared View × Proj matrix) instead of per-instance MVP. The shader reads Model from the storage buffer.
   - Push material UBO and bind textures as before (shared across instances).
   - Call `meshPtr->Draw(device, submission.InstanceCount)`.
3. For submissions with `InstanceCount == 1` (unbatched):
   - Keep existing path (push per-draw TransformUBO). No storage buffer bind needed.
   - OR: unify by also using the storage buffer path with a 1-element buffer. Simpler code, slight overhead.

**Recommendation:** Dual path — keep existing UBO push for single draws (zero overhead), use storage buffer path for batched draws. This avoids disrupting the single-draw hot path.

**Files:**
- `engine/wayfinder/src/rendering/passes/SubmissionDrawing.cpp` — fork on `InstanceCount > 1`
- `engine/wayfinder/src/rendering/passes/SubmissionDrawing.h` — may need updated state struct

### Step C2 — Shader Modifications
Modify vertex shaders to support instanced rendering via storage buffer.

**Changes for `textured_lit.vert` and `basic_lit.vert` (lit shaders with Model matrix):**
```hlsl
// New: shared view-projection (replaces per-draw MVP)
[[vk::binding(0, 1)]]
cbuffer ViewProjUBO : register(b0)
{
    float4x4 viewProj;
};

// New: per-instance storage buffer
[[vk::binding(0, 0)]]  // vertex stage, storage buffer slot 0
StructuredBuffer<float4x4> InstanceModels : register(t0);

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    float4x4 model = InstanceModels[instanceId];
    float4x4 mvp = mul(viewProj, model);
    
    output.Position = mul(mvp, float4(input.Position, 1.0));
    output.Normal = normalize(mul((float3x3)model, input.Normal));
    // ...
}
```

**Strategy:** Create instanced variants of shaders (`textured_lit_instanced.vert`, `basic_lit_instanced.vert`) rather than modifying the originals. The `ShaderProgramDesc` for each program can reference the instanced variant. The `DrawSubmission()` dual path selects the appropriate program.

**Alternative:** Use a single shader with a preprocessor define (`#ifdef INSTANCED`) or a push constant flag. DXC supports `-D INSTANCED` at compile time. Add instanced shader variants to CMake shader compilation.

**Recommended:** Separate instanced shader files. Cleaner, no runtime branching, easy to reason about. The ShaderProgramRegistry gets `"textured_lit_instanced"` as an additional program whose desc sets `numStorageBuffers = 1`.

**Files:**
- `engine/wayfinder/shaders/textured_lit_instanced.vert` — new file (copy of textured_lit.vert, modified for SV_InstanceID + storage buffer)
- `engine/wayfinder/shaders/basic_lit_instanced.vert` — new file
- `engine/wayfinder/shaders/unlit_instanced.vert` — new file (if unlit objects are batched)
- `cmake/WayfinderShaders.cmake` or the CMakeLists.txt shader list — add new shader files
- ShaderProgramRegistry registration — register instanced programs with `numStorageBuffers = 1` in vertex resources

### Step C3 — Program Selection for Instanced Draws
When `InstanceCount > 1`, `DrawSubmission()` must look up the instanced shader variant instead of the base program.

**Design:**
- `ShaderProgramDesc` could have an `InstancedVariant` string (name of the instanced program). Or:
- Convention: instanced variant name = `base_name + "_instanced"`. `DrawSubmission()` does: `programName = (instanceCount > 1) ? shaderName + "_instanced" : shaderName`.
- The instanced program must be registered in `ShaderProgramRegistry` with correct vertex resource counts (`numStorageBuffers = 1`).

**Files:**
- `engine/wayfinder/src/rendering/passes/SubmissionDrawing.cpp` — program name resolution
- Wherever built-in programs are registered (RenderOrchestrator or feature OnAttach) — register instanced variants

---

## Phase D: Verification & Testing

### Step D1 — Batch Merging Unit Tests
Test the `BatchSubmissions()` logic in isolation. Create curated submission lists and verify correct batching.

**Test cases:**
1. N identical mesh+material submissions → 1 batch of N instances
2. Mixed meshes with same material → separate batches per mesh
3. Same mesh with different materials → separate batches
4. Single submission → no batching (InstanceCount stays 1)
5. Submissions with HasOverrides=true → excluded from batching
6. Large run (100+ identical) → single batch with correct instance count
7. Interleaved mesh types → correct batch boundaries

**Files:**
- `tests/rendering/RenderOrchestratorTests.cpp` — add `SUBCASE("instance batching")` section

### Step D2 — Draw Call Recording for Integration Tests
Extend test infrastructure to verify that batched submissions produce fewer draw calls with correct instance counts.

**Design:**
- Add a `TrackingRenderDevice` (or enhance existing one in RenderFeatureTests) that records `DrawIndexed` calls: `{indexCount, instanceCount, firstIndex, vertexOffset}`.
- Also record `BindVertexStorageBuffers` calls to verify storage buffer binding.

**Files:**
- `tests/rendering/SubmissionDrawingTests.cpp` — add instanced draw tests
- `tests/TestHelpers.h` or a new `tests/rendering/TrackingDevice.h` — tracking device utility

### Step D3 — Visual & Performance Verification (Manual)
- Load a scene with many repeated meshes (e.g., forest of identical trees)
- Tracy: verify draw call count drops (e.g., 100 trees → 1 draw call instead of 100)
- RenderDoc: verify instanced draws show correct instance count and transforms
- Visual: all instances render at correct world positions

---

## Transparent Batching — Design Notes (Future)

Transparent geometry is back-to-front sorted, and draw order matters for correct blending. Instance batching breaks draw order. Possible approaches for future:
1. **Order-independent transparency (OIT):** If the engine adopts per-pixel linked lists or weighted blended OIT, draw order becomes irrelevant → batch freely.
2. **Depth-clustered batching:** Group transparent objects at similar depths into batches. Approximate but fast.
3. **Don't batch transparent:** Transparent draw counts are typically low (particles use their own system). May not be worth the complexity.

**Recommendation:** Don't batch transparent for now. Revisit if transparent draw counts become a bottleneck.

---

## Summary Table

| Step | Phase | Complexity | Dependencies | Can Parallelise? |
|------|-------|------------|--------------|-----------------|
| A1. BufferUsage flags | A | Low | None | Yes (with A2) |
| A2. BindVertexStorageBuffers | A | Low | None | Yes (with A1) |
| A3. Storage ring in TransientBufferAllocator | A | Low | A1 | After A1 |
| B1. BatchSubmissions() logic | B | Medium | A3 | No |
| B2. Material identity for batching | B | Low | None | Yes (with B1) |
| C1. Instanced DrawSubmission | C | Medium | A2, B1 | No |
| C2. Instanced shader variants | C | Low-Medium | None | Yes (with anything) |
| C3. Program selection | C | Low | C1, C2 | After C1+C2 |
| D1. Batch merging tests | D | Low | B1 | After B1 |
| D2. Draw call recording tests | D | Medium | C1 | After C1 |
| D3. Visual verification | D | Manual | All | Last |

**Critical path:** A1 → A3 → B1 → C1 → C3. Shaders (C2) can be written in parallel with everything.

---

## Relevant Files

- `engine/wayfinder/src/rendering/backend/RenderDevice.h` — BufferUsage, BindVertexStorageBuffers, ShaderCreateDesc
- `engine/wayfinder/src/rendering/backend/sdl_gpu/SDLGPUDevice.h` + `.cpp` — SDL_GPU implementations
- `engine/wayfinder/src/rendering/backend/null/NullDevice.h` — null stubs
- `engine/wayfinder/src/rendering/resources/TransientBufferAllocator.h` + `.cpp` — storage ring
- `engine/wayfinder/src/rendering/graph/RenderFrame.h` — RenderMeshSubmission fields
- `engine/wayfinder/src/rendering/pipeline/RenderOrchestrator.h` + `.cpp` — BatchSubmissions(), Prepare()
- `engine/wayfinder/src/rendering/passes/SubmissionDrawing.h` + `.cpp` — instanced draw path
- `engine/wayfinder/shaders/textured_lit_instanced.vert` — new instanced shader
- `engine/wayfinder/shaders/basic_lit_instanced.vert` — new instanced shader
- `tests/rendering/RenderOrchestratorTests.cpp` — batch merging tests
- `tests/rendering/SubmissionDrawingTests.cpp` — instanced draw tests
- `engine/wayfinder/CMakeLists.txt` — new source files
- `docs/plans/draw_model_architecture.md` — reference for storage buffer patterns

---

## Decisions

- **Storage buffer over instance-rate vertex buffer:** More extensible, no pipeline descriptor changes, aligns with engine roadmap.
- **Post-sort scan over sort key modification:** Material sort already clusters same-material submissions; mesh identity check within those groups catches batches. No depth precision loss.
- **Separate instanced shaders over preprocessor variants:** Cleaner separation, no runtime branching.
- **Dual draw path (UBO for singles, storage buffer for batches):** Avoids overhead on single draws which are still the common case for unique meshes.
- **Skip overridden materials from batching:** Simplifies equality check; overridden entities are rare.
- **Opaque only:** Transparent batching deferred; back-to-front ordering constraints conflict with instancing.
