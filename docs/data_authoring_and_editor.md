# Data Authoring And Editor Direction

## Purpose

This document explains how Wayfinder authored data currently works, what rules the runtime expects, and how the future editor should relate to that data.

The important principle is simple: the runtime owns scene semantics, and tools sit on top of them. Authoring rules should not exist only inside a future editor.

## Format Rules

- TOML is the primary hand-authored format
- JSON is reserved for generated, interchange, or diagnostic data
- authored files should remain readable by humans while the editor is still immature

## Asset Layout Today

The checked-in sandbox currently uses this structure:

- `sandbox/journey/assets/scenes/` for scenes
- `sandbox/journey/assets/prefabs/` for prefabs
- `sandbox/journey/assets/materials/` for material assets

The current bootstrap scene is `sandbox/journey/assets/scenes/default_scene.toml`.

## Scene Model

Scenes define entities through explicit component data. They should not rely on custom handwritten spawn logic for their normal content path.

The current scene format is intentionally narrow and centered around a top-level `entities` array.

Each entity may include:

- `id` as a stable scene object identifier
- `name` as a human-readable label
- `parent_id` to form hierarchy relationships
- `prefab_id` to reference a prefab asset
- `transform`
- `mesh`
- `material`
- `camera`
- `light`

Example from the current bootstrap scene:

```toml
scene_name = "Journey Bootstrap"

[[entities]]
id = "d43f13fe-fd18-46ad-9e2a-10d9cd0f6f9c"
name = "WorldRoot"

[entities.transform]
position = [0.0, 0.0, 0.0]
rotation = [0.0, 0.0, 0.0]
scale = [1.0, 1.0, 1.0]
```

## Identity Rules

Wayfinder separates human-facing names from persistent identity.

- names are for humans
- IDs are for references, persistence, overrides, and tooling
- runtime code uses typed scene and asset identifiers
- authored TOML serializes those IDs as UUID strings

This matters because hierarchy, prefab links, save/load symmetry, and future tooling all become fragile if names are treated as stable identity.

## Validation Rules

The runtime is intentionally strict about authored data.

Current expectations:

- unknown component tables are load errors
- malformed component payloads fail validation
- parent and prefab references must resolve correctly
- validation happens before the active scene is replaced
- save emits the same general component-table shape that load accepts

The top-level metadata fields matter:

- `name` is the human label for the entity
- `parent_id` is the authored hierarchy link
- `prefab_id` is the authored prefab reference
- component tables are reserved for real component payloads, not duplicated scene bookkeeping

This keeps the data path deterministic and makes headless validation meaningful.

## Prefab Model

Prefabs provide reusable component data with scene-local overrides.

The current checked-in prefab format looks like this:

```toml
asset_id = "f4a6f64d-7233-4d0c-bf13-cfd5f6f16b8c"
name = "MarkerCube"

[transform]
position = [0.0, 0.5, 0.0]
rotation = [0.0, 0.0, 0.0]
scale = [1.0, 1.0, 1.0]

[mesh]
primitive = "cube"
dimensions = [1.0, 1.0, 1.0]
color = [184, 184, 184, 255]
wireframe = true
```

Current prefab behavior:

- the asset registry resolves a prefab by `prefab_id`
- prefab component data is loaded first
- scene-local component data overrides prefab values afterward
- runtime-only derived state is never required in authored prefab data

## Material Asset Model

Material assets now provide a narrow authoring path for renderer-facing appearance data.

The current checked-in material asset format is intentionally small:

```toml
asset_id = "0c6ad84a-9a5f-4c44-ac34-b0f1387d99b4"
asset_type = "material"
name = "MarkerCubeMaterial"
base_color = [184, 184, 184, 255]
wireframe = true
```

Scene and prefab entities can reference a material asset through a `material` component table:

```toml
[material]
material_id = "0c6ad84a-9a5f-4c44-ac34-b0f1387d99b4"
base_color = [214, 78, 56, 255]
wireframe = true
```

Current material behavior:

- the asset registry distinguishes prefab and material assets
- material assets are validated headlessly before runtime scene mutation
- material asset defaults load first
- scene-local or prefab-local material values override asset defaults afterward

## Current Authoring Workflow

The practical workflow today is:

1. author or edit TOML scene and prefab files
2. validate the asset root with `waypoint validate-assets <asset-root>`
3. validate an individual scene with `waypoint validate <scene-path>`
4. optionally roundtrip-save with `waypoint roundtrip-save <scene-path> <output-path>`
5. run `journey` to inspect the runtime result

This matters because authoring must remain possible before Cartographer exists.

## Editor Direction

Cartographer should be a client of stable runtime and authoring semantics, not the source of those semantics.

Its first useful milestone is modest:

- open scene
- save scene
- hierarchy panel
- inspector panel
- viewport
- entity selection
- entity create and delete
- transform editing

Features that should wait until the basics are trustworthy:

- advanced gizmos
- undo and redo
- animation tooling
- graph tooling
- play-in-editor complexity

## Tooling Rules

These relationships should remain true as the project grows:

- `waypoint` owns the first serious headless validation workflow
- runtime and tooling code define schema behavior, not editor-only logic
- authored data should be loadable and verifiable without opening a GUI
- new component types should not be added faster than validation, save, and runtime consumption can keep up with them