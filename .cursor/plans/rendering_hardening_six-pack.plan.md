---
name: Rendering hardening six-pack
overview: Central GPU uniform layouts, primary view in frame params, graph pass capabilities with dev checks, ordering docs, naming rule for RenderPass, and typed well-known graph textures—implemented as clean replacements only (no deprecations, legacy shims, or migration scaffolding).
todos:
  - id: shader-uniforms-header
    content: Add ShaderUniforms.h as sole home for Transform/Unlit/DebugMaterial/SceneGlobals; remove duplicates; update all includes; CMake if needed
    status: completed
  - id: primary-view-params
    content: Add ResolvePreparedPrimaryView + PreparedPrimaryView; extend RenderPipelineFrameParams; set in Renderer; refactor SceneOpaque/Debug passes
    status: completed
  - id: capabilities-graph
    content: PassEntry capability mask + RenderGraphBuilder::DeclarePassCapabilities; call from SceneOpaque, Debug, composition; dev checks in Compile; update RenderPass docs
    status: completed
  - id: docs-ordering
    content: Expand render_passes.md (engine vs game vs composition, registration vs execution); optional workspace_guide link
    status: completed
  - id: naming-rule
    content: Document RenderPass vs I* in copilot-instructions + @note on RenderPass.h
    status: completed
  - id: typed-graph-textures
    content: GraphTextureId + FindHandle overloads; replace stringly WellKnown usage at call sites; update docs—remove old constants, no dual API
    status: completed
  - id: tests-lint
    content: Add/update doctest coverage; run ctest, lint.py, tidy.py
    status: completed
isProject: true
---

# Rendering pipeline hardening (six items)

## Principles (non-negotiable)

- **No deprecations, legacy paths, or migration notes in code or docs.** Replace the old approach with the new one; delete superseded APIs and update all call sites in the same change.
- **Single source of truth** for types and graph resource identity—no parallel “old name” / “new name” exports.

---

## 1. Centralise GPU uniform / UBO structs

**Add** [`engine/wayfinder/src/rendering/pipeline/ShaderUniforms.h`](engine/wayfinder/src/rendering/pipeline/ShaderUniforms.h) as the **only** definition site for:

- `UnlitTransformUBO`, `TransformUBO`, `DebugMaterialUBO`
- `SceneGlobalsUBO` (moved **out of** [`SubmissionDrawing.h`](engine/wayfinder/src/rendering/passes/SubmissionDrawing.h)—that header then `#include`s `ShaderUniforms.h` where needed, or consumers include `ShaderUniforms.h` directly; **no** typedef re-exports “for compatibility”)

**Remove** anonymous-namespace duplicates from [`ForwardOpaqueShaderPrograms.cpp`](engine/wayfinder/src/rendering/pipeline/ForwardOpaqueShaderPrograms.cpp), [`SubmissionDrawing.cpp`](engine/wayfinder/src/rendering/passes/SubmissionDrawing.cpp), [`DebugPass.cpp`](engine/wayfinder/src/rendering/passes/DebugPass.cpp). Keep `static_assert`s next to definitions where they encode GPU contracts.

Update [`engine/wayfinder/CMakeLists.txt`](engine/wayfinder/CMakeLists.txt) if headers are listed explicitly.

**Adjacent:** Use `UnlitTransformUBO` / `sizeof` consistently where [`DebugPass.cpp`](engine/wayfinder/src/rendering/passes/DebugPass.cpp) pushes raw `Matrix4` for lines if that matches the shader layout.

---

## 2. Primary prepared view + `RenderPipelineFrameParams`

- Add `PreparedPrimaryView` + `ResolvePreparedPrimaryView(const RenderFrame&)` (header next to frame types or small `RenderFrameUtils.h`).
- Extend [`RenderPipelineFrameParams`](engine/wayfinder/src/rendering/pipeline/RenderPipelineFrameParams.h) with `PreparedPrimaryView PrimaryView`.
- Set it in one place ([`Renderer::Render`](engine/wayfinder/src/rendering/pipeline/Renderer.cpp) after successful `Prepare`, or folded into `Prepare`—pick one owner).
- Refactor [`SceneOpaquePass`](engine/wayfinder/src/rendering/passes/SceneOpaquePass.cpp) and [`DebugPass`](engine/wayfinder/src/rendering/passes/DebugPass.cpp) to use `params.PrimaryView` only; **delete** duplicated `Views.front()` logic.

---

## 3. Capabilities: docs + graph metadata + `Compile` (dev)

- Clarify in [`RenderPass.h`](engine/wayfinder/src/rendering/graph/RenderPass.h) / [`RenderPassCapabilities.h`](engine/wayfinder/src/rendering/graph/RenderPassCapabilities.h): hints for scheduling/tooling until enforcement grows; optional graph metadata catches obvious mistakes in dev builds.
- `PassEntry` + `RenderGraphBuilder::DeclarePassCapabilities(RenderPassCapabilityMask)`; call from engine passes + composition lambda in [`RenderPipeline::BuildGraph`](engine/wayfinder/src/rendering/pipeline/RenderPipeline.cpp).
- Dev-only checks in [`RenderGraph::Compile`](engine/wayfinder/src/rendering/graph/RenderGraph.cpp) (conservative rules—account for `WriteColour(Load)` vs `ReadTexture`).

Game passes may omit declaration; no “legacy default” behaviour beyond a neutral unspecified state.

---

## 4. Document ordering

Expand [`docs/render_passes.md`](docs/render_passes.md): engine block → game block → composition; registration order vs dependency-driven execution. Link [`docs/workspace_guide.md`](docs/workspace_guide.md) if appropriate.

---

## 5. Naming: `RenderPass` vs `I*`

Update [`.github/copilot-instructions.md`](.github/copilot-instructions.md) and a short `@note` on [`RenderPass.h`](engine/wayfinder/src/rendering/graph/RenderPass.h). **Do not** rename `RenderPass` → `IRenderPass` unless you explicitly choose that later—this item is documentation-only.

---

## 6. Typed well-known graph textures (full replace)

- Add `enum class GraphTextureId` (or equivalent) and `std::string_view` name mapping.
- Add `RenderGraph::FindHandle` / `FindHandleChecked` overloads for that enum.
- **Remove** the old string-based `WellKnown::SceneColour` / `SceneDepth` API for these resources—update **all** engine call sites and tests to the enum API.
- Update [`docs/render_passes.md`](docs/render_passes.md) to describe the enum-first workflow only.

---

## Tests and validation

Add/update tests for uniforms, primary view resolution, graph enum lookup, and capability-related compile behaviour where practical (headless / `NullDevice` patterns). Run `ctest`, `tools/lint.py`, `tools/tidy.py`.

---

## Optional later (not part of this batch)

- Extract composition as its own injectable pass; multi-view primary selection—only when product needs them.
