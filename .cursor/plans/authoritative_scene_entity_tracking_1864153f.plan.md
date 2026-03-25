# Plan: Replace ClearEntities Fallback Sweep with Authoritative Scene Entity Tracking (#125)

## TL;DR

Replace the O(world) `m_world.each()` fallback sweep in `ClearEntities()` (and `SaveToFile()`) with a cached flecs relationship query scoped to `SceneOwnership(m_sceneTag)`, making the flecs relationship the single source of truth for scene entity ownership. Simultaneously, reject nil-ID assignment on scene-owned entities in `SetSceneObjectId` to prevent orphaning at the source. Maps become secondary lookup indexes only.

## Approach: Option C (both validation + query authority)

The issue recommends Option C and I agree—it's the right call. The flecs relationship already models ownership; promoting it to sole authority removes the dual-source-of-truth problem. Nil-ID validation prevents the orphaning bug class entirely, so the fallback sweep is no longer needed even conceptually.

---

## Steps

### Phase 1: Add Cached Flecs Query to Scene

1. **Add `flecs::query<>` member to `Scene`** in `Scene.h` — new member `m_ownedEntitiesQuery` alongside the existing maps. *(depends on nothing)*
2. **Build the query in the constructor** in `Scene.cpp` — after `m_sceneTag` creation, build: `m_ownedEntitiesQuery = m_world.query_builder<>().with<SceneOwnership>(m_sceneTag).build();` *(depends on step 1)*
3. **Clear/rebuild the cached query in `ClearEntities()`** — destroy `m_ownedEntitiesQuery` before collecting/destructing scene-owned entities, then rebuild it there for the non-shutdown path. This keeps query teardown aligned with the actual clear/reload path instead of relying on `Shutdown()` only. *(depends on step 2)*

### Phase 2: Refactor ClearEntities — Query as Authority

4. **Replace both phases of `ClearEntities()`** with a single query iteration. Remove the map iteration phase (`for (const auto& [id, entityId] : m_entitiesById)`) and the fallback sweep (`m_world.each(...)`). Replace with: destroy `m_ownedEntitiesQuery`, iterate a temporary ownership query, collect entities, destruct them, clear both maps, then rebuild `m_ownedEntitiesQuery` for the active scene path. `Shutdown()` should call `ClearEntities(false)` so it destroys the query without rebuilding it before `ReleaseSceneTag(...)` recycles the tag. *(depends on step 2)*
   - Collect into a vector first (avoid mutating during iteration).
   - Clear `m_entitiesById` and `m_entitiesByName` after destruction.

### Phase 3: Refactor SaveToFile — Use Same Query

5. **Replace `m_world.each()` in `SaveToFile()`** with a query iteration using `m_ownedEntitiesQuery`. Since SaveToFile is const, the query iteration must also work in a const context — flecs queries support const iteration via `.each()`. *(parallel with step 4)*

### Phase 4: Validate SetSceneObjectId — Reject Nil on Scene-Owned

6. **Change `Entity::SetSceneObjectId` to return `Result<void>`** — if the new ID is nil AND the entity has `SceneOwnership`, return an error. Non-scene entities (m_scene == nullptr or no SceneOwnership relationship) can still have nil IDs set. *(parallel with steps 4-5)*
   - Check: `m_entityHandle.has<SceneOwnership>(m_scene->GetSceneTag())` — need to expose `m_sceneTag` via a getter or friend access.
   - `Scene` already friends `Entity`, so a private getter `GetSceneTag()` is sufficient.
7. **Add `GetSceneTag()` accessor to `Scene`** — private, returns `flecs::entity`. Entity already has friend access. *(depends on nothing, parallel with step 6)*

### Phase 5: Tests

8. **Add test: SetSceneObjectId({}) on scene-owned entity is rejected** — verify the error is returned and the entity retains its original ID. *(depends on step 6)*
9. **Add test: ClearEntities destroys all scene-owned entities via relationship** — create entities, confirm they have SceneOwnership, call ClearEntities, verify all are gone. This exists partially but should verify no fallback sweep is needed. *(depends on step 4)*
10. **Add test: entity that had its SceneObjectId reassigned is still cleaned up on scene clear** — create entity, reassign ID, clear scene, verify destruction. *(depends on step 4)*
11. **Verify all existing scene and ECS tests pass** — run `wayfinder_scene_tests`. *(depends on steps 4-6)*

---

## Relevant Files

- [engine/wayfinder/src/scene/Scene.h](engine/wayfinder/src/scene/Scene.h) — Add `m_ownedEntitiesQuery` member, `GetSceneTag()` private accessor
- [engine/wayfinder/src/scene/Scene.cpp](engine/wayfinder/src/scene/Scene.cpp) — Refactor constructor (build query), `ClearEntities()`, `SaveToFile()`, `Shutdown()` (destroy query)
- [engine/wayfinder/src/scene/entity/Entity.cpp](engine/wayfinder/src/scene/entity/Entity.cpp) — Change `SetSceneObjectId` to return `Result<void>` with nil-ID validation
- [engine/wayfinder/src/scene/entity/Entity.h](engine/wayfinder/src/scene/entity/Entity.h) — Update `SetSceneObjectId` signature
- [tests/scene/ECSIntegrationTests.cpp](tests/scene/ECSIntegrationTests.cpp) — New tests for nil-ID rejection, query-based cleanup, ID-reassignment cleanup

## Verification

1. Build `wayfinder_scene_tests`: `cmake --build --preset debug --target wayfinder_scene_tests`
2. Run: `ctest --preset test -R scene` — all existing + new tests pass
3. Build `journey`: `cmake --build --preset debug --target journey` — no compile errors
4. Manual: confirm no `m_world.each` calls remain in Scene.cpp that filter by SceneOwnership

## Decisions

- **Query caching**: Use a persistent `flecs::query<>` on Scene rather than ad-hoc `filter_builder` or `each()`. Flecs 4.x caches query results internally, making iteration O(matched) not O(world). The query is built once in the constructor, cleared/rebuilt in `ClearEntities()`, and left destroyed on the shutdown path before `ReleaseSceneTag(...)` recycles the tag. Do not call `m_sceneTag.destruct()`; the tag is `ecs_clear`ed and pooled instead.
- **SetSceneObjectId return type**: Changed from `void` to `Result<void>`. This is a breaking API change but is consistent with the engine's error-handling convention and the project's greenfield stance on breaking changes.
- **SaveToFile also refactored**: Same O(world) pattern, same fix. Consistency + performance.
- **Maps remain mutable**: The `mutable` maps are kept as-is — they're still useful as O(1) lookup indexes. Their role just shifts from co-authority to pure index.
- **Scope boundary**: Only Scene entity ownership tracking is changed. No changes to SceneRenderExtractor's `each()` pattern (separate concern, separate issue).

## Further Considerations

1. **SceneRenderExtractor also uses `m_world.each()` with no relationship filter** — it iterates all world entities checking for components. This is a separate O(world) concern and should be a follow-up issue, not part of #125.
2. **Query const-ness**: Flecs `query::each()` works on non-const queries. Since `SaveToFile` is a const method, the query member may need to be `mutable` (flecs queries are safe to iterate from const contexts but the C++ wrapper may not mark iteration as const). Verify at implementation time and make `m_ownedEntitiesQuery` mutable if needed.
