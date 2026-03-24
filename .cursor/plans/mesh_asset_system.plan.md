# Plan: Mesh Asset System (Issue #30)

## TL;DR
Add a complete mesh asset pipeline: engine-native binary format (.wfmesh), MeshAsset type integrated into the existing asset system, GPU mesh caching via a new MeshManager, ECS MeshComponent extended with AssetId, glTF import via fastgltf + meshoptimizer in Waypoint, and multi-submesh/multi-vertex-format support. This is the first binary asset type and will establish the pattern for all future binary assets.

## Codebase Assessment

### What's solid and should be reused as-is:
- **AssetId / TypedId** — Type-safe UUID-based identification. Works exactly as needed.
- **AssetService / AssetRegistry / AssetCache<T>** — Clean, extensible. Template specialization pattern (AssetLoader<T>) is well-designed. Add AssetCache<MeshAsset> following TextureAsset as template.
- **AssetSchemaRegistry** — Function-pointer dispatch for validation. Just needs a new entry. Note: uses `std::array<Entry, 3>` — needs to grow to 4.
- **TextureManager pattern** — Good template for MeshManager: non-owning device pointer, cache by AssetId, GPU resource lifecycle, fallback resources. Follow this pattern closely.
- **RenderFrame submission model** — RenderMeshSubmission + RenderMeshRef already have Origin (BuiltIn/Asset) and StableKey, designed for this extension.
- **Material system** — MaterialAsset, parameter blocks, texture slots. Well-designed, no changes needed.

### What needs rework:
- **MeshComponent** — Currently `MeshPrimitive Primitive` (Cube) + `Dimensions` only. Extend with optional AssetId for asset meshes.
- **Mesh class** — Only has `CreatePrimitive()` / `CreateTexturedPrimitive()` factories. Already has a generic `Create(device, MeshCreateDesc)` path though, which is good. Extend to support arbitrary vertex data from MeshAsset.
- **SceneRenderExtractor** — Hardcoded to BuiltIn mesh origin with `K_BUILT_IN_BOX_MESH_KEY`. Needs asset mesh extraction path.
- **RenderResourceCache::ResolveMesh()** — Only handles built-in primitives. Needs to delegate to MeshManager for asset meshes.
- **AssetKind enum** — Needs `Mesh` value.
- **AssetSchemaRegistry** — Fixed `std::array<Entry, 3>` → needs to be 4, plus a `ValidateMeshDocument` function.

### What's NOT blocking but worth noting:
- No asset hot-reload, no streaming, no LOD — all deliberate scope exclusions for now.
- AssetRegistry scans only `.json` files — mesh binary is separate, referenced by JSON descriptor (same pattern as texture.json → texture.png).
- Waypoint is currently read-only (validate + roundtrip-save). Import is a new capability.

## Library Recommendations

### fastgltf (RECOMMENDED — primary importer)
- Modern C++17, clean API, very fast (simdjson under the hood)
- glTF 2.0 only (JSON + GLB) — focused, no bloat
- MIT license, well-maintained, active community
- CPM-compatible
- Ideal match for the engine's "focused, modern, no legacy" philosophy

### meshoptimizer (RECOMMENDED — post-processing companion)
- Vertex cache optimization, overdraw optimization, vertex deduplication, LOD simplification
- Industry standard (by Zeux/Arseny Kapoulkine), used in virtually every production engine
- MIT license, tiny footprint
- Ensures GPU-optimal vertex/index ordering in .wfmesh output

### Rejected alternatives:
- **Assimp** — Supports 40+ formats but quality is inconsistent, massive dependency, slow builds, known bugs in many paths. The multi-format support is bloat when glTF is the universal interchange format. Does not align with the engine's modular philosophy.
- **tinygltf** — Older design, slower than fastgltf, has stb_image dep (engine already uses SDL_image), less maintained.
- **cgltf** — Good C library, very minimal, but less idiomatic for C++23. More manual work for buffer extraction. Viable alternative if minimal deps are paramount.

## Binary Format Design (.wfmesh)

### File pair pattern (follows texture.json + texture.png):
- **mesh descriptor** (`.json`) — standard asset fields + source path + human-readable metadata
- **mesh binary** (`.wfmesh`) — self-contained vertex/index data with minimal structural header

### JSON descriptor:
```json
{
    "asset_id": "...",
    "asset_type": "mesh",
    "name": "my_mesh",
    "source": "my_mesh.wfmesh",
    "submeshes": [
        { "name": "body", "material_slot": 0 },
        { "name": "eyes", "material_slot": 1 }
    ]
}
```

### Binary format (.wfmesh):
```
Header (fixed):
  magic:    uint32  ("WFMH" = 0x484D4657)
  version:  uint16  (1)
  flags:    uint16  (reserved)
  submesh_count: uint32
  bounds:   AABB    (6 × float32 — whole-mesh bounding box)

Per-submesh table (contiguous array):
  vertex_format:      uint8   (enum: PosNormalUV, PosNormalUVTangent, etc.)
  index_format:       uint8   (Uint16 or Uint32)
  padding:            uint16
  vertex_count:       uint32
  index_count:        uint32
  vertex_data_offset: uint32  (byte offset from file start)
  vertex_data_size:   uint32
  index_data_offset:  uint32  (byte offset from file start)
  index_data_size:    uint32
  bounds:             AABB    (6 × float32 — submesh bounding box)
  material_slot:      uint32

Data blobs:
  [vertex data for submesh 0][vertex data for submesh 1]...
  [index data for submesh 0][index data for submesh 1]...
```

### Design decisions:
- **Single vertex stream per submesh for now.** The format supports adding a position-only stream later (via flags + additional offset fields in header v2). The rendering-side "position-only for shadow" can initially be handled by binding the same buffer with a position-only vertex layout — correct and simple, even if not bandwidth-optimal.
- **Submeshes map to material slots.** A mesh with 3 submeshes means up to 3 different materials. Initially, only single-material (slot 0) meshes are fully supported in the render path; multi-material is format-ready but deferred.
- **All offsets are from file start.** Enables direct memory-mapping in the future.
- **AABB bounds per submesh and whole mesh.** Enables frustum culling per submesh later.

## Steps

### Phase A: Asset Foundation

1. **Add fastgltf + meshoptimizer to build** — Add to `cmake/WayfinderDependencies.cmake` via CPM. fastgltf linked to waypoint only (import tool, not engine runtime). meshoptimizer linked to waypoint only. Verify Clang + Ninja builds.
   - Files: [cmake/WayfinderDependencies.cmake](cmake/WayfinderDependencies.cmake)

2. **Define binary format types** — Create `MeshFormat.h` with: `MeshFileHeader`, `SubMeshEntry`, `MeshVertexFormat` enum, AABB struct (or reuse from maths), magic/version constants, format read/write helpers.
   - Files: new `engine/wayfinder/src/assets/MeshFormat.h`, new `engine/wayfinder/src/assets/MeshFormat.cpp`

3. **Define MeshAsset + loader** — Create `MeshAsset.h/cpp` following TextureAsset pattern: struct with `Id`, `Name`, `SourcePath`, `SubMeshes[]` (metadata), `VertexData` / `IndexData` (raw bytes loaded from .wfmesh), plus `LoadMeshAssetFromDocument()` and `ValidateMeshAssetDocument()`. Specialize `AssetLoader<MeshAsset>`. The loader reads JSON descriptor → resolves .wfmesh path → reads binary file → parses header → stores vertex/index data in MeshAsset. Add `ReleaseGeometryData()` (like `ReleasePixelData()`) for CPU memory cleanup after GPU upload.
   - Files: new `engine/wayfinder/src/assets/MeshAsset.h`, new `engine/wayfinder/src/assets/MeshAsset.cpp`

4. **Register mesh in asset system** — Add `AssetKind::Mesh` to enum. Add entry to `AssetSchemaRegistry::GetEntries()` (update array size 3→4). Add `AssetCache<MeshAsset>` to `AssetService`. Add `LoadAsset<MeshAsset>` template specialization. Add `ReleaseMeshGeometryData(AssetId)` to AssetService.
   - Files: [engine/wayfinder/src/assets/AssetRegistry.h](engine/wayfinder/src/assets/AssetRegistry.h), [engine/wayfinder/src/assets/AssetSchemaRegistry.h](engine/wayfinder/src/assets/AssetSchemaRegistry.h), [engine/wayfinder/src/assets/AssetSchemaRegistry.cpp](engine/wayfinder/src/assets/AssetSchemaRegistry.cpp), [engine/wayfinder/src/assets/AssetService.h](engine/wayfinder/src/assets/AssetService.h), [engine/wayfinder/src/assets/AssetService.cpp](engine/wayfinder/src/assets/AssetService.cpp)

5. **Core tests for binary format** — Round-trip write/read of .wfmesh binary. MeshAsset loading from JSON descriptor + fixture binary. Validation tests for malformed files. Add test fixtures: minimal mesh JSON + .wfmesh binary.
   - Files: new test file in `tests/core/` or `tests/rendering/`, new fixtures in `tests/fixtures/`

### Phase B: Runtime Integration

6. **Create MeshManager** *(depends on step 3)* — Following TextureManager pattern: `Initialise(device)`, `GetOrLoad(assetId, assetService)` → returns GPU mesh, `Shutdown()`. Internals: `std::unordered_map<AssetId, GPUMesh>` cache. `GPUMesh` struct = vertex GPUBuffer + index GPUBuffer + index count + index element size + submesh metadata. Fallback: built-in unit cube mesh (like fallback checkerboard texture). Calls `assetService.ReleaseMeshGeometryData()` after GPU upload.
   - Files: new `engine/wayfinder/src/rendering/resources/MeshManager.h`, new `engine/wayfinder/src/rendering/resources/MeshManager.cpp`

7. **Wire MeshManager into RenderContext** *(depends on step 6)* — RenderContext owns MeshManager alongside TextureManager. Initialize in RenderContext::Initialize(), shutdown in Shutdown().
   - Files: [engine/wayfinder/src/rendering/RenderContext.h](engine/wayfinder/src/rendering/RenderContext.h), [engine/wayfinder/src/rendering/RenderContext.cpp](engine/wayfinder/src/rendering/RenderContext.cpp)

8. **Extend RenderResourceCache for mesh assets** *(depends on steps 6, 7)* — `ResolveMesh()` currently handles only built-in primitives. Add path: if `RenderMeshRef.Origin == Asset`, delegate to MeshManager. `SetMeshManager(MeshManager*)` non-owning pointer, following existing SetTextureManager pattern.
   - Files: [engine/wayfinder/src/rendering/resources/RenderResources.h](engine/wayfinder/src/rendering/resources/RenderResources.h), [engine/wayfinder/src/rendering/resources/RenderResources.cpp](engine/wayfinder/src/rendering/resources/RenderResources.cpp)

9. **Extend MeshComponent with AssetId** *(parallel with steps 6-8)* — Add `std::optional<AssetId> MeshAssetId`. If set, use asset mesh; otherwise fall back to primitive. Update serialization in `SceneComponentRegistry` (JSON: `"mesh_id": "uuid-string"`). Update validation.
   - Files: [engine/wayfinder/src/scene/Components.h](engine/wayfinder/src/scene/Components.h), scene component serialization files (ComponentRegistry / SceneComponentRegistry)

10. **Update SceneRenderExtractor** *(depends on step 9)* — In mesh extraction: if entity has `MeshAssetId`, set `RenderMeshRef.Origin = Asset`, `AssetId = meshAssetId`, `StableKey = MakeStableKey(assetId)`. Otherwise keep existing BuiltIn path.
   - Files: [engine/wayfinder/src/rendering/pipeline/SceneRenderExtractor.cpp](engine/wayfinder/src/rendering/pipeline/SceneRenderExtractor.cpp)

11. **Add VertexPosNormalUVTangent format** *(parallel with steps 6-10)* — New vertex struct with tangent (Float4, w = handedness). Add VertexLayouts entry. This is needed for normal-mapped meshes from glTF.
   - Files: [engine/wayfinder/src/rendering/backend/VertexFormats.h](engine/wayfinder/src/rendering/backend/VertexFormats.h)

12. **Update CMakeLists.txt** — Add all new source files to the engine library target.
   - Files: [engine/wayfinder/CMakeLists.txt](engine/wayfinder/CMakeLists.txt)

### Phase C: Import Pipeline + Validation

13. **Implement glTF import in Waypoint** *(depends on steps 1, 2, 3)* — New command: `waypoint import-mesh <gltf-path> <output-dir> [--name <name>]`. Implementation:
    - Parse glTF via fastgltf (handles .gltf + .glb)
    - Extract positions, normals, UVs, tangents, indices per primitive
    - Generate tangents if missing (via mikktspace or fastgltf utility)
    - Run meshoptimizer: vertex dedup → vertex cache optimise → overdraw optimise
    - Write .wfmesh binary (header + submesh table + vertex/index blobs)
    - Write .json descriptor (asset_id = newly generated UUID, source = relative .wfmesh path)
    - Log submesh count, vertex/index counts, format, bounds
   - Files: new `tools/waypoint/src/MeshImporter.h`, new `tools/waypoint/src/MeshImporter.cpp`, modify [tools/waypoint/src/WaypointMain.cpp](tools/waypoint/src/WaypointMain.cpp), modify `tools/waypoint/CMakeLists.txt`

14. **Add validate-mesh to Waypoint** *(depends on step 13)* — Extend existing `validate-assets` to recognize mesh JSON descriptors and validate the referenced .wfmesh binary (header check, bounds, data size consistency).
   - Files: [tools/waypoint/src/WaypointMain.cpp](tools/waypoint/src/WaypointMain.cpp)

15. **End-to-end tests** *(depends on all above)* — 
    - Core tests: MeshAsset load from fixture, binary round-trip fidelity, invalid file rejection
    - Render tests: MeshManager GetOrLoad with NullDevice, cache hit/miss, fallback mesh
    - Asset pipeline tests: validate-assets with mesh descriptors
    - Scene tests: MeshComponent with AssetId serialization round-trip
   - Files: add tests in `tests/core/`, `tests/rendering/`, `tests/scene/`

16. **Sandbox demo** *(depends on steps 8, 10, 13)* — Import a real glTF mesh (e.g., Suzanne from Blender, or a simple test mesh). Add to Journey sandbox assets. Add entity to default scene referencing the mesh by AssetId. Verify it renders in the sandbox.
   - Files: sandbox assets, sandbox scene files

## Relevant Files

**Extend (asset system):**
- [engine/wayfinder/src/assets/AssetRegistry.h](engine/wayfinder/src/assets/AssetRegistry.h) — add `AssetKind::Mesh`
- [engine/wayfinder/src/assets/AssetSchemaRegistry.h](engine/wayfinder/src/assets/AssetSchemaRegistry.h) / [.cpp](engine/wayfinder/src/assets/AssetSchemaRegistry.cpp) — add mesh validation entry, grow array
- [engine/wayfinder/src/assets/AssetService.h](engine/wayfinder/src/assets/AssetService.h) / [.cpp](engine/wayfinder/src/assets/AssetService.cpp) — add MeshAsset cache + LoadAsset specialization
- [engine/wayfinder/src/assets/TextureAsset.h](engine/wayfinder/src/assets/TextureAsset.h) — **reference pattern** for MeshAsset design

**Extend (rendering):**
- [engine/wayfinder/src/rendering/resources/RenderResources.h](engine/wayfinder/src/rendering/resources/RenderResources.h) / [.cpp](engine/wayfinder/src/rendering/resources/RenderResources.cpp) — `ResolveMesh()` asset path
- [engine/wayfinder/src/rendering/resources/TextureManager.h](engine/wayfinder/src/rendering/resources/TextureManager.h) — **reference pattern** for MeshManager design
- [engine/wayfinder/src/rendering/RenderContext.h](engine/wayfinder/src/rendering/RenderContext.h) / [.cpp](engine/wayfinder/src/rendering/RenderContext.cpp) — own MeshManager
- [engine/wayfinder/src/rendering/backend/VertexFormats.h](engine/wayfinder/src/rendering/backend/VertexFormats.h) — add PosNormalUVTangent
- [engine/wayfinder/src/rendering/mesh/Mesh.h](engine/wayfinder/src/rendering/mesh/Mesh.h) / [.cpp](engine/wayfinder/src/rendering/mesh/Mesh.cpp) — existing Mesh class (may refactor or use alongside)

**Extend (ECS/scene):**
- [engine/wayfinder/src/scene/Components.h](engine/wayfinder/src/scene/Components.h) — `MeshComponent.MeshAssetId`
- [engine/wayfinder/src/rendering/pipeline/SceneRenderExtractor.cpp](engine/wayfinder/src/rendering/pipeline/SceneRenderExtractor.cpp) — asset mesh extraction
- Scene component serialization files — mesh_id JSON field

**Extend (tools):**
- [tools/waypoint/src/WaypointMain.cpp](tools/waypoint/src/WaypointMain.cpp) — `import-mesh` command
- [cmake/WayfinderDependencies.cmake](cmake/WayfinderDependencies.cmake) — fastgltf + meshoptimizer

**New files:**
- `engine/wayfinder/src/assets/MeshAsset.h` / `.cpp`
- `engine/wayfinder/src/assets/MeshFormat.h` / `.cpp`
- `engine/wayfinder/src/rendering/resources/MeshManager.h` / `.cpp`
- `tools/waypoint/src/MeshImporter.h` / `.cpp`
- Test files + fixtures

## Verification

1. `cmake --preset dev` succeeds with new dependencies (fastgltf, meshoptimizer)
2. `ctest --preset test` — all existing + new tests pass
3. `waypoint import-mesh <test.gltf> <output-dir>` — produces valid .json + .wfmesh pair
4. `waypoint validate-assets <dir-with-mesh>` — mesh assets pass validation
5. Manual: Journey sandbox renders at least one imported mesh alongside existing primitive cubes
6. Binary round-trip test: write .wfmesh → read back → verify header, vertex/index data byte-exact
7. NullDevice test: MeshManager loads asset, caches, returns valid handle, frees CPU data
8. Scene round-trip: save scene with mesh AssetId → reload → verify MeshComponent preserved

## Decisions

- **fastgltf over Assimp/tinygltf/cgltf** — modern C++, fast, focused, CPM-friendly. glTF is the interchange format; no need for 40+ format support.
- **meshoptimizer for post-processing** — industry standard, ensures GPU-optimal data.
- **Both libraries linked to waypoint only** — engine runtime never touches raw glTF. Clean separation: import tool produces engine-native format, engine only reads its own format.
- **JSON descriptor + binary .wfmesh pair** — follows existing texture pattern (JSON + image file). Keeps AssetRegistry scanner working (only scans .json).
- **Single vertex stream per submesh initially** — format header has room for position-only stream in v2. Shadow pass uses same buffer with position-only vertex layout for now (correct, just not bandwidth-optimal).
- **Multi-submesh in format, single-material in renderer initially** — the binary format and MeshAsset support multiple submeshes. The renderer and MeshComponent initially support submesh 0 / material slot 0 only. Multi-material rendering is a separate follow-up.
- **MeshComponent uses optional<AssetId>** — if set, asset mesh; if not, fall back to existing primitive. Backwards-compatible with all existing scenes.

## Further Considerations

1. **Tangent generation** — glTF meshes may lack tangents. fastgltf doesn't generate them. Options: (A) bundle mikktspace (small, standard) in waypoint for tangent gen, (B) skip tangent-less meshes for normal mapping, (C) compute tangents at import time from UV gradients. Recommend (A).
2. **Index format (16-bit vs 32-bit)** — Meshes with >65535 vertices need 32-bit indices. The import pipeline should auto-select based on vertex count. Current engine uses Uint16 only — `IndexElementSize::Uint32` needs to be supported in the render device. Check if SDL_GPU / SDLGPUDevice already handles Uint32 indices.
3. **Mesh LOD / streaming** — Not in scope, but the .wfmesh format should be designed so that LOD levels can be added in a v2 header extension without breaking v1 readers. The current design's version + flags fields support this.
