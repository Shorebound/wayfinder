# Draw Model Architecture

> Decision record for Wayfinder's rendering draw model.
> Captures the analysis of flat draw-list vs GPU-driven rendering,
> scene-scale projections, and the incremental migration path.

## Decision

**Flat draw-list with per-submesh draw items.** One `RenderMeshSubmission` = one draw call = one material binding. The extraction layer decomposes multi-submesh mesh assets into N flat draw items. The renderer executes a sorted flat list without nested loops.

This is the Bevy/Frostbite model, not the Godot/Unity-legacy model.

---

## The Flat Draw-List Model

### Core Principle

The renderer's job is to execute a sorted flat list of draw calls. Everything upstream exists to produce that list.

This is a convergence point across Frostbite (frame graphs + draw packets), Destiny's renderer (draw items in a flat bucket), Our Machinery (flat draw call arrays), and Bevy. Engines that deviate — Unity's legacy Renderer with per-object multi-pass, Godot's multi-surface iteration — consistently struggle with cross-object state sorting, batching, and instancing, and end up adding SRP/RenderGraph layers to flatten things anyway.

### Why, From First Principles

**Sorting.** A scene has entity A with submeshes (metal, glass) and entity B with submeshes (metal, wood). Optimal draw order groups both metal submeshes together (one pipeline bind), then glass, then wood. This is only possible if metal-A and metal-B are separate items in the same sortable list. If they're trapped inside their parent entity's multi-surface object, entities sort but surfaces don't — producing metal, glass, metal, wood instead of metal, metal, glass, wood. The overhead scales with scene complexity.

**Batching and instancing.** The atomic unit for GPU instancing is (vertex buffer + pipeline state + material). With flat draw items, grouping identical items is a sort + run detection. With multi-surface meshes, you decompose first — doing at draw time what you could have done at extraction time.

**Culling.** Per-submesh AABB culling is free with flat draw items (each carries its own bounds). Multi-surface meshes either cull the whole object or need per-surface culling inside the draw loop.

**GPU-driven rendering (future).** The path to `DrawIndexedIndirect` with a compute-generated command buffer is to flatten everything into a draw-descriptor buffer. Flat draw items are already that buffer. Multi-surface meshes must be decomposed first.

### Data Flow

```
Scene Layer (one component per entity):
  MeshComponent { MeshAssetId, MaterialSlotBindings }

GPU Layer (all submeshes stored):
  MeshAssetGPU { vector<Mesh> Submeshes, vector<uint32_t> MaterialSlots }

Extraction (decomposition point):
  for each entity with MeshComponent:
      gpu_asset = mesh_manager.Get(entity.mesh_id)
      for each submesh in gpu_asset:
          material = resolve(entity.material_slots[submesh.slot])
          emit DrawItem { submesh.gpu_handle, material, transform, sort_key, bounds }

Renderer (flat loop):
  sort(draw_items)
  for each item in draw_items:
      bind_pipeline_if_changed(item.pipeline)
      bind_mesh_if_changed(item.mesh)
      push_uniforms(item.transform, item.material)
      draw()
```

The scene layer preserves the "one entity, one mesh" mental model. The renderer stays dumb. The extraction layer is the only piece of code that knows "a mesh asset has N submeshes."

### Why Not Multi-Surface (Godot Model)

- **Sorting breaks.** One submission with 3 surfaces at different depths can't produce a single correct sort key. You'd split into separate draws anyway.
- **Material resolution gets complex.** Each submission currently carries one `RenderMaterialBinding`. Multi-surface means resolving materials inside the draw loop or carrying a parallel vector of bindings.
- **Instancing gets harder.** Grouping identical submesh+material pairs for instanced rendering is trivial when each is its own submission. Multi-surface meshes require decomposition first.

---

## GPU-Driven Rendering

### What It Is

In a GPU-driven renderer (Assassin's Creed Unity, Wihlidal's Frostbite talk, Nanite), the CPU uploads a buffer of all drawable things to the GPU. A compute shader does culling, LOD selection, and sorting, then writes an indirect draw command buffer. The CPU never issues individual draw calls — it issues one `DrawIndexedIndirect` (or `MultiDrawIndirect`) that the GPU populated.

Key data structures:

```
// CPU uploads once per frame:
struct DrawDescriptor {       // lives in GPU buffer
    uint32_t MeshletOffset;   // or VB/IB offset into mega-buffer
    uint32_t IndexCount;
    uint32_t MaterialIndex;   // index into bindless material table
    uint32_t TransformIndex;  // index into transform buffer
    AABB     Bounds;          // for GPU frustum/occlusion cull
};

// GPU compute shader produces:
struct IndirectDrawCommand {
    uint32_t IndexCount;
    uint32_t InstanceCount;   // 0 if culled
    uint32_t FirstIndex;
    int32_t  VertexOffset;
    uint32_t FirstInstance;
};
```

### Direct Comparison

| Dimension | Flat draw-list | GPU-driven |
|---|---|---|
| **Draw call overhead** | One API call per draw item. ~10K draws before CPU-bound on modern APIs. | One (or handful of) indirect calls. CPU cost nearly constant. |
| **Culling** | CPU-side frustum/occlusion, per draw item. Good for low thousands. | GPU compute per object/meshlet. Handles millions. |
| **State changes** | Sort by pipeline+material to minimise. Still N binds for N distinct states. | Bindless — materials/textures are indices. Zero state changes. |
| **Mesh data** | Separate VB/IB per mesh. Bind per draw. | All meshes in one mega-buffer. Zero binds. |
| **LOD** | CPU selects LOD before submission. | GPU compute selects LOD per-object or per-meshlet (Nanite-style). |
| **Complexity** | Straightforward. A junior dev can follow the draw loop. | Substantial. Requires bindless, buffer device addresses, indirect draw, compute, careful sync. |
| **API requirements** | Works on any API including SDL_GPU. | Needs Vulkan/D3D12/Metal 3 features. SDL_GPU supports `DrawIndexedIndirect` but not bindless. |
| **Memory model** | Independent mesh allocations. Simple lifetime. | Mega-buffer with suballocation, compaction, defragmentation. |
| **Debugging** | RenderDoc shows individual draws with clear state. | RenderDoc shows one giant indirect call. Debugging requires custom tooling. |

### Why Purely GPU-Driven Is Wrong for Wayfinder

**SDL_GPU backend.** SDL_GPU abstracts over Vulkan/D3D12/Metal but doesn't expose bindless textures or buffer device addresses. Bypassing the abstraction for the most impactful parts of GPU-driven defeats its purpose.

**"Performance with clarity."** GPU-driven sacrifices clarity. Indirection layers (material tables, transform buffers, draw command generation in compute) make the rendering path opaque. The project principle says to optimise measured bottlenecks.

**"Engine is a library."** GPU-driven is harder to extend from the game side. Adding a custom material or render pass means understanding material table layouts and descriptor indexing. Flat draw-list: push a submission struct and everything works.

**Scene scale ceiling.** Sixth-gen fidelity means the vertex cost per object is trivially cheap. The bottleneck is draw-call count, and even at WoW-scale scene density (see below), CPU instancing handles it.

---

## Scene Scale Analysis: WoW-Style Open World

### Real-World Numbers

WoW in 2004 rendered roughly 1,000–3,000 visible objects per frame in a typical zone, with LOD and aggressive view-distance culling. Characters were ~1K triangles, buildings a few hundred to a few thousand.

With sixth-gen fidelity on modern hardware, per-object vertex cost is trivially cheap, so you'd push object density higher:

- **Dense vegetation:** Grass clumps, bushes, flowers, scattered rocks — hundreds to thousands in view.
- **Prop-heavy towns:** Market stalls, crates, barrels, hanging signs, NPCs, furniture — 500+ in a busy area.
- **Terrain chunks:** Chunked LOD terrain — 50–200 draw calls.
- **Particles/effects:** Torches, campfires, spell effects, ambient dust.

Realistic estimate for a dense zone: **3,000–10,000 draw items per frame** after frustum culling. High end (dense forest + village + many NPCs): maybe 15,000.

### Draw Call Budget

Each draw call on modern APIs costs roughly:

- **Vulkan/D3D12 via SDL_GPU:** ~2–5 µs per state-change + draw.
- At 5 µs/draw and 16.6 ms budget (60 fps): **~3,300 draws max** before CPU saturation.
- At 2 µs/draw (well-sorted, minimal state changes): **~8,300 draws**.

At 10K–15K draw items, a naive per-draw loop **will** become the bottleneck. CPU instancing collapses this.

### Draw Budget After Instancing

In a WoW-style scene, most draw items are repeated props. 2,000 grass clumps with the same mesh + material = 1 instanced draw call with a per-instance transform buffer. After instancing, 10,000 draw items collapse to ~200–500 actual API calls. Comfortably within budget at 60 fps.

### Skeletal Characters

Characters are fundamentally different from static props for draw-call budgeting. Static props are identical geometry at different positions — trivially instanceable. A skeletal character has a unique pose every frame (unique bone matrices), which changes the instancing calculus.

#### Character Counts

WoW's character density varies dramatically by context:

| Scenario | Characters in view | Notes |
|---|---|---|
| Open-world questing | 10–50 | Sparse NPCs, few other players |
| Town hub (Stormwind) | 50–200 | Guards, vendors, quest givers, players |
| Busy capital (Ironforge AH) | 200–500 | Peak player congregation |
| Raid encounter | 25 players + 20–60 adds | 45–85 total, complex animation |
| Massive PvP / world event | 500–1,000+ | WoW buckles here; frame rate collapses |

At sixth-gen fidelity, a character is ~500–2,000 triangles with 2–4 submeshes (body, armour pieces, weapon, cloak/cape). Each character = **2–6 draw items** before instancing.

#### Cost Breakdown

**Few hundred characters (200):**

- 200 characters × 3 submeshes = **600 draw items**
- Added to 3,000–10,000 static scene items → 3,600–10,600 total draw items
- Without instancing: 600 character draws at 5 µs = 3 ms. Significant but manageable

**Few thousand characters (2,000):**

- 2,000 characters × 3 submeshes = **6,000 draw items**
- Added to scene items → 9,000–16,000 total draw items
- Without instancing: 6,000 character draws at 5 µs = **30 ms**. Exceeds entire frame budget at 60 fps. Instancing becomes mandatory

#### Can Characters Be Instanced?

It depends entirely on the skinning approach.

**CPU skinning** (transform vertices on CPU, upload as unique per-character geometry): Every character is unique post-skin geometry. **Not instanceable.** Each character is a completely separate draw call. This is the simplest implementation but the worst for scalability.

**GPU skinning with bone palette** (vertex shader reads bone matrices from a buffer): All characters sharing the same base mesh submit the same vertex buffer (bind-pose geometry). The vertex shader applies per-instance bone transforms read from a shared bone matrix buffer. **Instanceable.** The draw call groups by (mesh, material) and each instance indexes into its own bone palette slice.

```
// Per-instance data for GPU-skinned instanced draw:
struct SkinnedInstance {
    Matrix4 WorldTransform;
    uint32_t BonePaletteOffset;  // index into bone matrix SSBO
};

// 200 human_male_body characters with the same material
// → 1 instanced draw, 200 entries in the instance buffer
// → 200 bone palette slices in the bone matrix SSBO
```

**Compute skinning** (compute shader pre-skins into an output vertex buffer): A compute pass transforms bind-pose vertices per-character into a shared output buffer. The draw calls then reference offsets into this buffer. Batching is possible but not traditional instancing — each character's skinned vertices live at different offsets.

#### Mesh Sharing and Equipment

WoW achieves its character density by exploiting mesh reuse. All human males share the same base skeletal mesh. Equipment changes appearance via texture swaps and **attachment-point meshes**, not by modifying the base character geometry. This is the key insight that makes the whole system scale.

##### Why Attachment Meshes Are Easier Than Character Modification

The alternative to attachment meshes is modifying the character mesh itself — swapping out vertex regions, merging armour geometry into the body mesh, or maintaining per-outfit mesh variants. That approach destroys instancing: every character with a different equipment loadout becomes unique geometry, which means a unique draw call. With 200 characters and 50 different gear combinations, you get 200 separate draws for the body alone.

Attachment meshes avoid this entirely. The base body mesh is *never modified*. Equipment is separate geometry that either:

1. **Rigid-attaches to a bone** — weapons, shields, rings, necklaces. These inherit a single bone's world transform. No skinning needed. They're static meshes with a parent transform, and they enter the flat draw-list as ordinary static draw items.
2. **Skin-conforms to the skeleton** — gloves, boots, torso armour, helmets. These are skinned meshes with their own vertex weights referencing the same skeleton. They use the same bone palette as the character they're equipped on, making them instanceable independently of the base body.

The base body mesh stays identical for every character of the same race/gender, so it remains fully instanceable. Equipment pieces are *also* instanceable — 50 warriors wearing the same iron helmet = 1 instanced draw for that helmet mesh.

When gear covers a body region, the corresponding body submesh is simply hidden (not drawn — skipped at extraction time). Wearing a helmet hides the hair submesh. Wearing gloves hides the bare-hand submesh. No geometry modification, no rebinding, just toggling which submeshes the extraction layer emits.

##### Full Outfit: Draw Item Budget

A fully equipped character with visible equipment in every slot adds more draw items per character:

| Component | Mesh type | Skinning | Notes |
|---|---|---|---|
| Body base | Skinned (2–3 submeshes: torso, legs, face) | Yes | Hidden submeshes toggled by gear |
| Helmet | Skinned (conforms to head) | Yes | Hides hair submesh |
| Shoulders | Skinned or rigid | Depends | Often rigid-attached to shoulder bones |
| Torso armour | Skinned (conforms to chest/arms) | Yes | Hides body torso submesh |
| Bracers | Skinned | Yes | Small, low-poly |
| Gloves | Skinned | Yes | Hides bare-hand submesh |
| Belt | Skinned or rigid | Depends | Often 1 bone (hip) |
| Pants / leg armour | Skinned | Yes | Hides body legs submesh |
| Boots | Skinned | Yes | Hides bare-foot submesh |
| Weapon(s) | Rigid-attached to hand bone | No | Static mesh, parent transform only |
| Shield | Rigid-attached to off-hand bone | No | Static mesh |
| Ring / necklace | Rigid-attached or hidden | No | Often not rendered, or very small |

**Worst case:** ~10–14 draw items per fully equipped character (body submeshes that aren't hidden + all visible gear pieces). This is significantly more than the 2–4 estimate for a simple character.

**What changes with full outfits:**

1. **Draw items per character triple.** 2,000 characters × 12 draw items = **24,000 draw items** before instancing. Without instancing, this is completely unaffordable (~120 ms at 5 µs/draw).

2. **Instancing still saves you.** Equipment is shared more than you'd expect. MMOs have a finite loot table — in any given zone, you'll see maybe 30–100 unique equipment mesh variants across all visible characters. So the instancing collapse is dramatic:

   | Item | Unique meshes in view | Characters wearing | Instance count |
   |---|---|---|---|
   | Iron helmet | 1 mesh | 40 characters | 1 instanced draw, 40 instances |
   | Leather chest | 1 mesh | 25 characters | 1 instanced draw, 25 instances |
   | Steel greatsword | 1 mesh | 30 characters | 1 instanced draw, 30 instances |

   The 24,000 draw items collapse based on how many unique (mesh, material) pairs exist, not how many characters exist. Realistically: **200–600 instanced draws** for 2,000 fully equipped characters.

3. **Material variety increases.** Each gear piece may have its own material (different texture atlas region or separate texture). More unique materials = more instancing groups = more actual draw calls. This is the real cost of full outfits — not vertex count, but material fragmentation. Mitigated by texture atlasing (one material for all plate armour variants, indexed by UV offset) or material merging at the asset level.

4. **Skinned vs. rigid split.** Rigid-attached items (weapons, shields, rings) don't need bone palettes. They're ordinary static draw items with a parent transform looked up from a bone. This means they sort and instance separately from skinned gear — which is fine, it just means more instance groups. The flat draw-list handles it naturally.

5. **Bone palette size grows slightly.** Full-body armour pieces reference the same skeleton as the base body — they don't add new bones. But if gear has auxiliary bones (dangling belt pouch, swaying cape), those extend the palette. Budget ~60–80 bones per character instead of ~40–60 for the base skeleton.

6. **Body-part toggling is extraction-time, zero-cost at draw time.** When the extractor sees a character wearing a helmet, it simply doesn't emit the hair submesh. No GPU cost, no conditional branching in the shader. This is one of the benefits of the flat draw-list: the decision about *what to draw* is made upstream, and the renderer never sees the hidden pieces.

##### Revised Character Budget (Full Outfits)

| Character count | Draw items (12/char) | Without instancing | With GPU skinning + instancing |
|---|---|---|---|
| 50 (sparse overworld) | ~600 | 600 draws (3 ms) | ~40–80 draws |
| 200 (town hub) | ~2,400 | 2,400 draws (12 ms) | ~80–150 draws |
| 500 (busy capital) | ~6,000 | 6,000 draws (30 ms) — **budget blown** | ~150–300 draws |
| 2,000 (massive event) | ~24,000 | 24,000 draws — unaffordable | ~200–600 draws |

Full outfits make instancing even more essential — the budget blows at 500 characters without it, down from ~1,000 with simple characters. But with GPU skinning + instancing, even 2,000 fully equipped characters land comfortably within the draw budget.

The takeaway: **CPU skinning works up to ~200 characters. Beyond that, GPU skinning with instancing is not optional.** For a WoW-scale open world at sixth-gen fidelity, GPU skinning should be the default path from the start.

#### Impact on the Flat Draw-List

Skeletal characters fit cleanly into the flat draw-list model. The extraction layer:

1. Reads the entity's skeletal mesh component and current animation pose
2. Emits N draw items per character (one per submesh), each tagged with a bone palette index
3. The draw items enter the same flat list as static meshes, sorted by (pipeline, material, mesh)
4. The instancing pass groups characters with the same (mesh, material) into instanced draws — each instance carries its bone palette offset

The vertex shader branches on whether the draw uses skinning or not (or you use separate pipelines for static vs skinned — a per-pipeline decision). The flat draw-list doesn't care; it's just another draw item with an extra per-instance field.

### Scene Scale Summary (Including Characters)

| Scene scale | Architecture needed | Static items | Characters (fully equipped) | Total draws (after instancing) |
|---|---|---|---|---|
| Small indie | Flat draw-list | ~100–500 | ~10 | ~100–500 |
| Medium | Flat draw-list | ~500–2,000 | ~50 | ~500–2,000 |
| **WoW-style, sixth-gen** | **Flat draw-list + instancing + frustum cull + GPU skinning** | **3K–10K → 200–500 draws** | **200–2,000 → 80–600 draws** | **~280–1,100 actual draws** |
| AAA open world | GPU-driven | 100K+ | 1K+ | Indirect draws |

---

## GPU Workloads: What Goes Where

Not all work that *could* run on the GPU *should*. At Wayfinder's scene scale, the right placement for each workload depends on frequency, data shape, and whether the result feeds multiple consumers.

### Placement Principles

- **Per-vertex, needed once per draw** → vertex shader. No new passes, no synchronisation.
- **Per-entity or needed in multiple passes** → compute. Skin once, draw in colour + shadow + reflection.
- **Small item counts where the result stays on CPU** → CPU. Sort keys, LOD selection at low thousands, animation state machines.
- **Data-parallel and stateful across frames** → compute. Particle simulation, physics integration, anything that reads last frame's output.

### Workload Table

| Workload | Best Home | Rationale |
|---|---|---|
| **Skinning (initial)** | Vertex shader | Zero extra passes. Same draw model. Per-vertex bone transform is trivially parallel. |
| **Skinning (multi-pass)** | Compute | Skin once, draw N times (colour + shadow + reflection). Avoids redundant VS work. |
| **Frustum culling** | CPU (Stage 2), then compute (Stage 5+) | At 3–15K items, CPU AABB test is ~0.15 ms. Move to compute only if draw descriptor buffer is already on GPU. |
| **Occlusion culling** | Compute | Hierarchical-Z readback or GPU occlusion queries. Only relevant at AAA density. |
| **Sort key generation** | CPU | Sorting ~3–15K items is sub-millisecond. Not worth the GPU round-trip. |
| **Instance buffer packing** | CPU (Stage 4), then compute | CPU detects runs in sorted list, writes compact instance buffer. Compute version only useful when the draw list itself lives on GPU. |
| **Particle simulation** | Compute | Stateful: position, velocity, lifetime → write to vertex buffer → draw instanced quads. Textbook compute workload. |
| **LOD selection** | CPU | At WoW scale with LODs, CPU distance checks at extraction time. Moves to compute only in GPU-driven path. |
| **GPU-driven draw generation** | Compute + indirect | Compute culls and writes `DrawIndexedIndirect` argument buffer. Not needed at Wayfinder's target scale. |
| **Morph targets / blend shapes** | Compute or vertex shader | Similar tradeoff to skinning. VS for single-pass, compute when shared across passes. |

---

## GPU Skinning Architecture

GPU skinning is the critical enabler for character-heavy scenes. It makes skinned characters instanceable (same mesh + material = one instanced draw regardless of pose) and keeps vertex data on the GPU instead of re-uploading per-character per-frame.

### Approach 1: Vertex Shader Skinning (Recommended First)

The vertex shader reads bone matrices from a storage buffer (SSBO) and transforms each vertex by its bone weights. All characters sharing the same base mesh submit the *same* bind-pose vertex buffer. Per-instance data carries an offset into the bone matrix buffer.

```hlsl
// skinned_lit.vert (HLSL → SPIR-V)

[[vk::binding(0, 1)]]
cbuffer TransformUBO : register(b0) {
    float4x4 viewProjection;
};

// Bone matrices for ALL visible characters, packed contiguously.
// Each character's palette starts at instance.BonePaletteOffset.
[[vk::binding(0, 2)]]
StructuredBuffer<float4x4> BoneMatrices : register(t0);

struct VSInput {
    float3 position  : POSITION;
    float3 normal    : NORMAL;
    float4 weights   : BLENDWEIGHT;   // bone weights (up to 4)
    uint4  indices   : BLENDINDICES;  // bone indices (up to 4)
    // ... tangent, uv, etc.
};

struct InstanceData {
    float4x4 world;
    uint     bonePaletteOffset;  // index into BoneMatrices
};

float4x4 ComputeSkinMatrix(uint4 boneIndices, float4 boneWeights, uint paletteOffset) {
    return BoneMatrices[paletteOffset + boneIndices.x] * boneWeights.x
         + BoneMatrices[paletteOffset + boneIndices.y] * boneWeights.y
         + BoneMatrices[paletteOffset + boneIndices.z] * boneWeights.z
         + BoneMatrices[paletteOffset + boneIndices.w] * boneWeights.w;
}

VSOutput VSMain(VSInput input, InstanceData instance) {
    float4x4 skinMatrix = ComputeSkinMatrix(
        input.indices, input.weights, instance.bonePaletteOffset);
    float4 skinnedPos = mul(skinMatrix, float4(input.position, 1.0));
    // ... transform by world and viewProjection
}
```

**Why this first:**
- Does not change the draw model at all. One `DrawIndexed` per submesh — same flat draw-list.
- No extra render passes, no GPU synchronisation.
- Only requires binding a storage buffer during the render pass (one new RenderDevice method).
- Instancing works: group characters by (mesh, material), each instance carries its bone offset.

**SDL_GPU implementation:**

The path from current code to vertex shader skinning requires two additions to `RenderDevice`:

1. **General-purpose buffer creation.** Currently `CreateBuffer` hardcodes `SDL_GPU_BUFFERUSAGE_VERTEX` or `SDL_GPU_BUFFERUSAGE_INDEX`. Bone matrix SSBOs need `SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ`. The buffer creation API should accept usage flags directly:

   ```cpp
   // Current (too narrow):
   info.usage = (desc.usage == BufferUsage::Vertex)
       ? SDL_GPU_BUFFERUSAGE_VERTEX
       : SDL_GPU_BUFFERUSAGE_INDEX;

   // Needed: support for storage buffers
   // BufferUsage::GraphicsStorage → SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ
   // Can also combine: Vertex | GraphicsStorage for buffers used as both
   ```

2. **Vertex storage buffer binding.** SDL_GPU has `SDL_BindGPUVertexStorageBuffers()` which binds SSBOs readable from vertex shaders. This needs a corresponding `RenderDevice::BindVertexStorageBuffers()` method:

   ```cpp
   // New RenderDevice method:
   virtual void BindVertexStorageBuffers(
       GPUBufferHandle* buffers, uint32_t count, uint32_t firstSlot = 0) = 0;

   // SDL_GPU implementation:
   void SDLGPUDevice::BindVertexStorageBuffers(...) {
       // Convert handles to SDL_GPUBuffer* array
       // Call SDL_BindGPUVertexStorageBuffers(m_renderPass, firstSlot, bindings, count);
   }
   ```

The shader side (declaring `StructuredBuffer<float4x4>`) compiles through the existing Slang → SPIR-V pipeline. `ShaderCreateDesc` already has `numStorageBuffers` for declaring the binding count.

The upload path is identical to any other buffer: create transfer buffer, map, copy bone matrices, unmap, upload via copy pass. The `TransientBufferAllocator` could be extended to support per-frame storage buffer allocation for bone palettes (they change every frame).

### Approach 2: Compute Shader Skinning

A compute pass reads bind-pose vertices + bone matrices from storage buffers, writes pre-skinned vertices into an output buffer. The render pass draws from the output buffer as a normal vertex buffer.

```
Frame flow:
  1. Upload bone matrices (per-character palettes)
  2. Compute pass: skin all characters
     - Input:  bind-pose VB (read-only), bone matrices (read-only)
     - Output: skinned VB (read-write)
  3. Render pass: draw from skinned VB
```

**Advantages over vertex shader skinning:**
- Skin once, use in all passes (colour, shadow, reflection). With VS skinning, each pass re-skins.
- Reduces per-draw vertex shader cost in multi-pass rendering.
- Output buffer can serve collision/physics queries (CPU readback if needed).

**Disadvantages:**
- Doubles vertex memory: bind-pose buffer + skinned output buffer.
- Adds a frame dependency: compute must finish before the render pass reads the output.
- Breaks simple instancing — each character's skinned vertices live at different offsets, so you can't share a single vertex buffer binding. (You *can* still batch via multi-draw or buffer offsets.)
- More infrastructure work.

**SDL_GPU implementation gaps for compute skinning:**

The compute pipeline infrastructure exists (`AddComputePass`, `CreateComputePipeline`, `DispatchCompute`), but two plumbing gaps block real compute work:

1. **`BeginComputePass()` doesn't bind resources.** The current implementation calls `SDL_BeginGPUComputePass(cmdBuf, nullptr, 0, nullptr, 0)` — no storage buffers or textures. SDL_GPU requires storage bindings at pass creation time (not after, unlike render passes). The signature needs to accept `SDL_GPUStorageBufferReadOnlyBinding` and `SDL_GPUStorageTextureReadWriteBinding` arrays:

   ```cpp
   // Current:
   void SDLGPUDevice::BeginComputePass() {
       m_computePass = SDL_BeginGPUComputePass(m_commandBuffer,
           nullptr, 0, nullptr, 0);  // no bindings
   }

   // Needed:
   void SDLGPUDevice::BeginComputePass(
       std::span<const StorageTextureBinding> storageTextures,
       std::span<const StorageBufferBinding> readOnlyBuffers,
       std::span<const StorageBufferBinding> readWriteBuffers);
   ```

2. **Buffer usage flags (same as VS skinning).** Buffers used in compute need `SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ` or `_WRITE`. The current `CreateBuffer` only produces vertex or index buffers.

These two gaps also block every other compute workload (particles, GPU culling, etc.), so fixing them is foundational — not skinning-specific.

### Recommendation

**Start with vertex shader skinning.** It requires fewer changes (one new buffer usage flag, one new binding call), doesn't change the draw model, and enables instancing. Move to compute skinning when multi-pass rendering makes redundant VS skinning measurably expensive. The two approaches are not mutually exclusive — you can use VS skinning for single-pass and compute skinning for shadow/reflection passes.

---

## SDL_GPU Infrastructure Status

Summary of what the rendering backend supports today and what needs to be added for each migration stage.

### What Works Today

| Capability | Status | Key code |
|---|---|---|
| Vertex / index buffer create + upload | Working | `SDLGPUDevice::CreateBuffer`, `UploadToBuffer` |
| Per-frame transient buffer allocation | Working | `TransientBufferAllocator` — ring buffers for dynamic geometry |
| Shader compilation (Slang → SPIR-V) | Working | `slangc` + `WayfinderShaders.cmake`, `ShaderManager` loads `.spv` |
| Storage buffer declaration on shaders | Working | `ShaderCreateDesc::numStorageBuffers`, `ShaderResourceCounts` |
| Render graph with compute passes | Working | `RenderGraph::AddComputePass()`, tested in `RenderGraphTests` |
| Compute pipeline create/destroy | Working | `CreateComputePipeline` wraps `SDL_CreateGPUComputePipeline` |
| Compute dispatch | Working | `DispatchCompute(groupX, groupY, groupZ)` |

### What Needs To Be Added

| Gap | Blocks | Effort | Detail |
|---|---|---|---|
| **General buffer usage flags** | VS skinning, compute skinning, particles, instancing | Small | Extend `BufferUsage` enum and `CreateBuffer` to support `GraphicsStorageRead`, `ComputeStorageRead`, `ComputeStorageWrite`, and combinations thereof. Map to `SDL_GPUBufferUsageFlags`. |
| **`BindVertexStorageBuffers()`** | VS skinning | Small | New `RenderDevice` virtual method → `SDL_BindGPUVertexStorageBuffers()`. |
| **`BindFragmentStorageBuffers()`** | Material parameter buffers, PBR texture indices | Small | Same pattern, wraps `SDL_BindGPUFragmentStorageBuffers()`. |
| **Compute pass resource binding** | All real compute workloads | Medium | Extend `BeginComputePass()` to accept storage buffer and texture bindings. SDL_GPU requires these at pass-begin time. |
| **Instanced draw support** | CPU-side instancing (Stage 4) | Medium | `DrawIndexedInstanced` with per-instance vertex buffer. SDL_GPU already supports this via a second vertex buffer binding with `input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE`. Need `BindVertexBuffer` to support multiple slots. |
| **Indirect draw** | GPU-driven (Stage 5) | Medium | `SDL_DrawGPUIndexedPrimitivesIndirect()`. Plus argument buffer creation and compute-to-draw synchronisation. |

### Dependency Graph

```
General buffer usage flags ─┬─→ BindVertexStorageBuffers ────→ VS Skinning (Stage 3)
                            ├─→ BindFragmentStorageBuffers ──→ Future material system
                            ├─→ Compute pass resource binding → Compute Skinning
                            │                                 → Particle Sim
                            │                                 → GPU Frustum Cull
                            └─→ Instanced draw support ──────→ CPU Instancing (Stage 4)
                                                              → Skinned Instancing

Indirect draw ──────────────────────────────────────────────→ GPU-Driven (Stage 5)
```

---

## Migration Path

The flat draw-list is not a dead end. It's a stepping stone where each stage is incremental and none requires rearchitecting the previous one.

### Stage 1: Flat Draw-List (Current Target)

Flat draw items, CPU sort, per-draw API calls. Handles small-to-medium scene complexity trivially. No instancing, no culling beyond what the extraction layer skips.

### Stage 2: Frustum Culling

CPU-side frustum test per draw item using two-tier sphere→AABB rejection. Cost: ~10 ns/object. 50K objects = 0.5 ms. Essential for open-world scenes — without it the flat list balloons with off-screen objects.

**Implemented:**
- `BoundingSphere` + `AxisAlignedBounds` per `RenderMeshSubmission`, populated during extraction.
- Arvo's fast AABB transform (Graphics Gems 1990) in `TransformBounds` — ~3x faster than 8-corner brute force.
- `Frustum::TestBounds` two-tier test: cheap sphere pre-rejection (6 dot products), then tighter AABB p-vertex test only for sphere-visible items.
- `RenderView` pre-computes `ViewMatrix`, `ProjectionMatrix`, and `ViewFrustum` once per view in `Prepare()` — no per-pass recomputation.
- Camera `NearPlane`/`FarPlane` are data-driven on the `Camera` struct, not hard-coded.
- Cull → sort ordering in `Prepare()`: dead submissions are removed before sorting, so sort operates on the visible set only.

**Future improvements:**
- **ECS visibility writeback.** After culling, write a `ViewVisibility` component (or tag) back to the ECS world so gameplay systems, animation, audio, and particle emitters can skip work for off-screen entities. Requires an entity reference on each submission.
- **Hierarchical culling.** For dense scenes, a spatial acceleration structure (BVH or octree) can reject entire subtrees before testing individual items.
- **Temporal coherence.** Cache visibility results across frames and only re-test entities near frustum edges.

### Stage 3: GPU Skinning

Skeletal characters submit bind-pose geometry; the vertex shader applies per-instance bone transforms from a shared bone matrix SSBO. See [GPU Skinning Architecture](#gpu-skinning-architecture) for the full design and SDL_GPU implementation details.

**Infrastructure needed:**
- Extend `BufferUsage` enum → add `GraphicsStorageRead`
- Extend `CreateBuffer` → map to `SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ`
- Add `RenderDevice::BindVertexStorageBuffers()` → wraps `SDL_BindGPUVertexStorageBuffers()`
- Add `BLENDWEIGHT` / `BLENDINDICES` to `VertexFormats.h`
- Write `skinned_lit.slang` shader (Slang → SPIR-V, same pipeline as existing shaders)
- Per-frame bone palette upload via `TransientBufferAllocator` or dedicated bone buffer

**Requires on the CPU side:** skeleton hierarchy component, animation keyframe evaluation, bone matrix palette generation. These are animation system concerns — the GPU plumbing is independent.

### Stage 4: CPU-Side Instancing

After sorting draw items by (pipeline, material, mesh), detect runs of identical (mesh + material) and emit one instanced draw call per run with a per-instance transform buffer. Works for both static props and GPU-skinned characters (which share the same bind-pose VB). The single biggest performance win for prop-heavy and character-heavy scenes.

**Infrastructure needed:**
- `BindVertexBuffer` must support multiple buffer slots (slot 0 = geometry, slot 1 = per-instance data). SDL_GPU handles this via `SDL_GPUBufferBinding` with `SDL_GPU_VERTEXINPUTRATE_INSTANCE`.
- Sort key must encode mesh identity so identical draws are adjacent. Current `SortKeyBuilder` encodes (layer, material hash, depth, priority) — mesh ID bits should be added.

### Stage 5: Compute Workloads

Fix the two compute infrastructure gaps (buffer usage flags + compute pass resource binding) to enable real compute work. This unblocks:

- **Compute skinning** — skin once, draw in colour + shadow + reflection passes.
- **Particle simulation** — stateful position/velocity/lifetime, writes to vertex buffer.
- **Any future GPU-side processing** — morph targets, cloth sim, procedural geometry.

**Infrastructure needed:**
- Extend `BeginComputePass()` to accept storage buffer/texture binding arrays
- Extend `BufferUsage` → add `ComputeStorageRead`, `ComputeStorageWrite`
- Compile first `.comp` shaders through Slang → SPIR-V (same build pipeline; stage profile TBD)

### Stage 6: GPU-Driven (If Needed)

Upload draw descriptor buffer to GPU, add frustum cull compute pass, use `DrawIndexedIndirect`. The data is already flat — you're moving the loop from CPU to GPU. Merge meshes into a mega-buffer, add bindless materials.

**Requires:** `SDL_DrawGPUIndexedPrimitivesIndirect()` support, bypassing SDL_GPU's abstraction for bindless resources (or the abstraction gaining that support). Substantial complexity increase. Only justified if CPU instancing proves insufficient.

### Design-Now, Implement-Later

Even at Stage 1, these should be part of the data model to avoid rework later:

1. **Bounds on draw items.** ~~Propagate `AxisAlignedBounds` from mesh asset submeshes to `RenderMeshSubmission`.~~ Done — submissions carry both `WorldBounds` (AABB) and `WorldSphere` (bounding sphere), populated during extraction. Frustum culling uses both in a two-tier test.
2. **Mesh identity in sort key.** Add mesh ID bits to `SortKeyBuilder` so identical meshes are adjacent after sorting. Enables Stage 4 without sort-key rework.
3. **General buffer usage flags.** Even before skinning or compute, extending `BufferUsage` beyond Vertex/Index costs nothing and unblocks all future GPU work.

---

## Position on the Spectrum

```
CPU-driven (per-draw)  →  Flat draw-list  →  GPU-driven (indirect)
       ↑                       ↑                      ↑
   Unity legacy           Wayfinder               Nanite/Frostbite
```

The flat draw-list is the CPU-side version of exactly what a GPU-driven renderer uploads to the GPU. GPU-driven is architecturally a superset, not an alternative — you build towards it, you don't choose it instead. 
