#include "DebugPass.h"

#include "rendering/backend/RenderDevice.h"
#include "rendering/backend/VertexFormats.h"
#include "rendering/graph/RenderCapabilities.h"
#include "rendering/graph/RenderFrameUtils.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/pipeline/BuiltInUBOs.h"
#include "rendering/pipeline/RenderServices.h"
#include "rendering/resources/TransientBufferAllocator.h"

#include "core/Log.h"
#include "maths/Maths.h"

#include <algorithm>

#ifdef WAYFINDER_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4324)
#endif

namespace Wayfinder
{
    namespace
    {
        /// Extracts a Float4 base_colour from a debug box's material parameters.
        /// Falls back to opaque white when the parameter is absent.
        Float4 ExtractBoxColour(const RenderDebugBox& box)
        {
            const auto it = box.Material.Parameters.Values.find("base_colour");
            if (it != box.Material.Parameters.Values.end())
            {
                if (const auto* c = std::get_if<LinearColour>(&it->second))
                {
                    return c->Data;
                }
            }
            return Float4(1.0f);
        }
    } // namespace

    void DebugPass::AppendWorldGridLineVertices(std::vector<VertexPositionColour>& lineVertices, const WorldGridSpec spec)
    {
        const int clamped = std::max(1, spec.Slices);
        const float extent = static_cast<float>(clamped) * spec.Spacing;
        const Float3 majorColour{0.45f, 0.45f, 0.45f};
        const Float3 minorColour{0.25f, 0.25f, 0.25f};

        for (int i = -clamped; i <= clamped; ++i)
        {
            const float coord = static_cast<float>(i) * spec.Spacing;
            const Float3& gridColour = (i == 0) ? majorColour : minorColour;

            lineVertices.push_back({.Position = Float3{-extent, 0.0f, coord}, .Colour = gridColour});
            lineVertices.push_back({.Position = Float3{extent, 0.0f, coord}, .Colour = gridColour});
            lineVertices.push_back({.Position = Float3{coord, 0.0f, -extent}, .Colour = gridColour});
            lineVertices.push_back({.Position = Float3{coord, 0.0f, extent}, .Colour = gridColour});
        }
    }

    std::span<const ShaderProgramDesc> DebugPass::GetShaderPrograms() const
    {
        static const auto PROGRAMS = []
        {
            std::vector<ShaderProgramDesc> p;
            p.reserve(2);

            // Lines and grid: PosColour, drawn as LineList via GetVariantPipeline.
            {
                ShaderProgramDesc desc;
                desc.Name = "debug_unlit";
                desc.VertexShaderName = "debug_unlit";
                desc.FragmentShaderName = "debug_unlit";
                desc.VertexResources = {.UniformBuffers = 1};
                desc.FragmentResources = {.UniformBuffers = 1};
                desc.VertexLayout = VertexLayouts::POSITION_COLOUR;
                desc.Cull = CullMode::None;
                desc.DepthTest = false;
                desc.DepthWrite = false;
                p.push_back(std::move(desc));
            }

            // Solid debug geometry (boxes): PosNormalColour to match BuiltInMeshId::PrimitiveColour.
            {
                ShaderProgramDesc desc;
                desc.Name = "debug_solid";
                desc.VertexShaderName = "unlit";
                desc.FragmentShaderName = "unlit";
                desc.VertexResources = {.UniformBuffers = 1};
                desc.FragmentResources = {.UniformBuffers = 1};
                desc.VertexLayout = VertexLayouts::POSITION_NORMAL_COLOUR;
                desc.Cull = CullMode::Back;
                desc.DepthTest = false;
                desc.DepthWrite = false;
                p.push_back(std::move(desc));
            }

            return p;
        }();

        return PROGRAMS;
    }

    void DebugPass::OnAttach(const RenderFeatureContext& context)
    {
        m_context = &context.Context;
    }

    void DebugPass::OnDetach(const RenderFeatureContext& /*context*/)
    {
        m_context = nullptr;
    }

    void DebugPass::AddPasses(RenderGraph& graph, const FrameRenderParams& params)
    {
        if (!m_context)
        {
            Log::Warn(LogRenderer, "DebugPass: no context -- skipped");
            return;
        }

        // Overlay must write the same colour target CompositionPass will sample (post chain), not SceneColour.
        const RenderGraphHandle colourHandle = ResolvePostProcessInput(graph);

        // ── Build per-view debug draw data (single pass over layers) ─────────
        auto& transientAllocator = m_context->GetTransientBuffers();

        std::array<PerViewDebugDraw, MAX_DEBUG_VIEWS> viewDraws{};
        uint32_t viewCount = 0;
        m_scratchBoxes.clear();

        for (const auto& layer : params.Frame.Layers)
        {
            if (!layer.Enabled || !layer.DebugDraw)
            {
                continue;
            }

            const auto& debugDraw = *layer.DebugDraw;
            const size_t viewIdx = layer.ViewIndex;

            // Find or create view slot.
            PerViewDebugDraw* viewData = nullptr;
            for (uint32_t v = 0; v < viewCount; ++v)
            {
                if (viewDraws[v].ViewIndex == viewIdx)
                {
                    viewData = &viewDraws[v];
                    break;
                }
            }
            if (!viewData)
            {
                if (viewCount >= MAX_DEBUG_VIEWS)
                {
                    continue;
                }
                viewData = &viewDraws[viewCount++];
                viewData->ViewIndex = viewIdx;
            }

            // Accumulate line vertices into this view's scratch buffer.
            if (debugDraw.ShowWorldGrid)
            {
                AppendWorldGridLineVertices(viewData->ScratchLines, {.Slices = debugDraw.WorldGridSlices, .Spacing = debugDraw.WorldGridSpacing});
            }

            for (const auto& line : debugDraw.Lines)
            {
                const Float3 lineColour = LinearColour::FromColour(line.Tint).ToFloat3();
                viewData->ScratchLines.push_back({.Position = line.Start, .Colour = lineColour});
                viewData->ScratchLines.push_back({.Position = line.End, .Colour = lineColour});
            }

            // Accumulate boxes into per-view scratch (flattened later).
            if (!debugDraw.Boxes.empty())
            {
                viewData->ScratchBoxes.insert(viewData->ScratchBoxes.end(), debugDraw.Boxes.begin(), debugDraw.Boxes.end());
            }
        }

        // Flatten per-view boxes into the shared scratch buffer with correct start/count.
        for (uint32_t v = 0; v < viewCount; ++v)
        {
            auto& vd = viewDraws[v];
            if (!vd.ScratchBoxes.empty())
            {
                vd.BoxStart = static_cast<uint32_t>(m_scratchBoxes.size());
                vd.BoxCount = static_cast<uint32_t>(vd.ScratchBoxes.size());
                m_scratchBoxes.insert(m_scratchBoxes.end(), vd.ScratchBoxes.begin(), vd.ScratchBoxes.end());
                vd.ScratchBoxes.clear();
            }
        }

        // Upload line vertices per view.
        for (uint32_t v = 0; v < viewCount; ++v)
        {
            auto& vd = viewDraws[v];
            vd.LineVertexCount = static_cast<uint32_t>(vd.ScratchLines.size());
            if (!vd.ScratchLines.empty())
            {
                const auto dataSize = static_cast<uint32_t>(vd.ScratchLines.size() * sizeof(VertexPositionColour));
                vd.LineAlloc = transientAllocator.AllocateVertices(vd.ScratchLines.data(), dataSize);
            }
            vd.ScratchLines.clear(); // Free memory; data is on GPU now.
        }

        // m_scratchBoxes persists until the next AddPasses call (next frame).
        const RenderDebugBox* boxData = m_scratchBoxes.data();

        graph.AddPass("Debug", [&, viewDraws, viewCount, boxData](RenderGraphBuilder& builder)
        {
            builder.DeclarePassCapabilities(RenderCapabilities::RASTER | RenderCapabilities::RASTER_OVERLAY_OR_DEBUG);
            auto depth = graph.FindHandleChecked(GraphTextureId::SceneDepth);
            builder.WriteColour(colourHandle, LoadOp::Load);
            builder.WriteDepth(depth, LoadOp::Load);

            return [this, &params, viewDraws, viewCount, boxData](RenderDevice& device, const RenderGraphResources& /*resources*/)
            {
                if (!m_context)
                {
                    return;
                }

                auto& registry = m_context->GetPrograms();
                const GPUPipelineHandle linePipeline = registry.GetVariantPipeline("debug_unlit", PrimitiveType::LineList);
                const ShaderProgram* solidProgram = registry.Find("debug_solid");
                const Mesh* boxMesh = params.BuiltInMeshes[static_cast<size_t>(BuiltInMeshId::PrimitiveColour)];

                // ── Draw lines ───────────────────────────────
                if (linePipeline.IsValid())
                {
                    device.BindPipeline(linePipeline);

                    for (uint32_t v = 0; v < viewCount; ++v)
                    {
                        const auto& vd = viewDraws[v];
                        if (vd.LineVertexCount == 0 || !vd.LineAlloc.IsValid())
                        {
                            continue;
                        }

                        const auto resolved = Rendering::ResolveViewForLayer(params, vd.ViewIndex);
                        if (!resolved.IsValid)
                        {
                            continue;
                        }

                        const UnlitTransformUBO transformUBO{.Mvp = resolved.ProjectionMatrix * resolved.View};
                        const DebugMaterialUBO materialUBO{.BaseColour = Float4(1.0f)};

                        device.BindVertexBuffer(vd.LineAlloc.Buffer, {.OffsetInBytes = vd.LineAlloc.Offset});
                        device.PushVertexUniform(0, &transformUBO, sizeof(UnlitTransformUBO));
                        device.PushFragmentUniform(0, &materialUBO, sizeof(DebugMaterialUBO));
                        device.DrawPrimitives(vd.LineVertexCount);
                    }
                }

                // ── Draw boxes ───────────────────────────────
                if (!solidProgram || !solidProgram->Pipeline.IsValid() || !boxMesh || !boxMesh->IsValid())
                {
                    return;
                }

                bool hasPendingBoxes = false;
                for (uint32_t v = 0; v < viewCount; ++v)
                {
                    if (viewDraws[v].BoxCount > 0)
                    {
                        hasPendingBoxes = true;
                        break;
                    }
                }

                if (!hasPendingBoxes)
                {
                    return;
                }

                device.BindPipeline(solidProgram->Pipeline);
                boxMesh->Bind(device);

                for (uint32_t v = 0; v < viewCount; ++v)
                {
                    const auto& vd = viewDraws[v];
                    if (vd.BoxCount == 0)
                    {
                        continue;
                    }

                    const auto resolved = Rendering::ResolveViewForLayer(params, vd.ViewIndex);
                    if (!resolved.IsValid)
                    {
                        continue;
                    }

                    for (uint32_t b = 0; b < vd.BoxCount; ++b)
                    {
                        const auto& box = boxData[vd.BoxStart + b]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                        const UnlitTransformUBO transformUBO{.Mvp = resolved.ProjectionMatrix * resolved.View * box.LocalToWorld};
                        const DebugMaterialUBO materialUBO{.BaseColour = ExtractBoxColour(box)};

                        device.PushVertexUniform(0, &transformUBO, sizeof(UnlitTransformUBO));
                        device.PushFragmentUniform(0, &materialUBO, sizeof(DebugMaterialUBO));
                        boxMesh->Draw(device);
                    }
                }
            };
        });
    }

#ifdef WAYFINDER_COMPILER_MSVC
#pragma warning(pop)
#endif

} // namespace Wayfinder
