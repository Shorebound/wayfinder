# Rendering Performance Goals

**Status:** Planned
**Last updated:** 2026-04-02
**Related:** [render_passes.md](../render_passes.md), [application_architecture_v2.md](application_architecture_v2.md)

Target architecture and constraints for the ECS-to-GPU hot path. Detailed implementation (BVH choice, frame arena API, instancing buffer layout, GPU-driven rendering) is future work.

---

## Pipeline Stages

```
ECS World
  |
  1. Visibility       spatial query (BVH/grid) against frustum
  |
  2. Extract           convert visible entities to render proxies
  |                    (mesh handle, material handle, transform, LOD level)
  |                    frame-allocated, no heap per-entity
  |
  3. Sort              radix sort by material, mesh, depth
  |
  4. Batch             merge same-mesh-same-material runs into instanced draws
  |                    per-instance transform buffer
  |
  5. Submit            minimal draw calls, GPU-driven where possible
```

---

## Design Constraints

- **No per-entity `Has<T>()` / `Get<T>()` in the render loop.** Use flecs archetype-bulk queries. The current `SceneRenderExtractor` walks entities individually -- this must be replaced.
- **No per-frame material lookups.** Materials are resolved to handles at load time or on change (observer). The extractor reads handles, not string IDs.
- **Frame-allocated render data.** `RenderProxy` structs are bump-allocated from a per-frame arena. No `std::vector<>` resize per frame, no heap churn.
- **Spatial indexing for visibility.** A BVH, grid, or octree that returns only visible entities. Without this, extraction touches every entity regardless of camera position.
- **Instancing by default.** Identical mesh + material combinations are merged into instanced draws. A forest of 10K identical trees becomes a handful of draw calls.
- **LOD selection during extraction.** The extractor picks LOD level based on screen-space size before emitting the render proxy. No LOD switching in GPU passes.

---

## Parallelism Opportunities

| Stage | Approach |
|---|---|
| Visibility | Read-only queries, splittable by spatial partition across threads |
| Extraction | Parallelisable per archetype (contiguous memory per archetype) |
| Sort | Radix sort is inherently parallelisable |
| Batch | Sequential scan of sorted keys, but operates on cache-friendly sorted arrays |

Double-buffered extraction is the primary threading model: the current frame's extraction writes to a frame-owned arena while the GPU reads last frame's completed data.

---

## Current State vs Target

| Aspect | Current | Target |
|---|---|---|
| Entity iteration | Per-entity `Has<T>()` / `Get<T>()` | Archetype-bulk flecs queries |
| Material resolution | Per-frame string/name lookup | Handle-based, resolved at load |
| Submission data | `std::vector<RenderSubmission>` | Frame-arena bump-allocated proxies |
| Visibility | None (all entities extracted) | Spatial index (BVH/grid) |
| Instancing | None | Automatic batching of identical draws |
| LOD | None | Screen-space selection during extraction |
| Threading | Single-threaded | Visibility + extraction parallelisable |

---

## Priority

This is the highest-impact performance work in the renderer. The architectural plumbing (frame arenas, handle-based materials, spatial index integration) needs to be designed into the extraction pipeline from the start -- retrofitting is significantly harder than building it in.
