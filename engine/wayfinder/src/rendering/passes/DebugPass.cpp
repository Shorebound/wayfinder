#include "DebugPass.h"

#include "rendering/backend/RenderDevice.h"
#include "rendering/backend/VertexFormats.h"
#include "rendering/graph/RenderCapabilities.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/pipeline/BuiltInUBOs.h"
#include "rendering/pipeline/PipelineCache.h"
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
        struct ResolvedViewForLayer
        {
            Matrix4 View{};
            Matrix4 Proj{};
            bool Ok = false;
        };

        /**
         * @brief Resolves view/projection for a layer — same rules as \ref SceneOpaquePass (primary defaults, optional per-view override).
         */
        ResolvedViewForLayer ResolveViewMatricesForLayer(const FrameRenderParams& params, size_t viewIndex)
        {
            ResolvedViewForLayer r;
            const auto& primary = params.PrimaryView;
            r.View = primary.ViewMatrix;
            r.Proj = primary.ProjectionMatrix;
            if (viewIndex < params.Frame.Views.size() && params.Frame.Views.at(viewIndex).Prepared)
            {
                const auto& pv = params.Frame.Views.at(viewIndex);
                r.View = pv.ViewMatrix;
                r.Proj = pv.ProjectionMatrix;
                r.Ok = true;
                return r;
            }
            r.Ok = primary.Valid;
            return r;
        }
    } // namespace

    void DebugPass::AppendWorldGridLineVertices(std::vector<VertexPosColour>& lineVertices, const WorldGridSpec spec)
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

    void DebugPass::OnAttach(const RenderFeatureContext& context)
    {
        m_context = &context.Context;

        GPUPipelineDesc desc{};
        desc.vertexShaderName = "debug_unlit";
        desc.fragmentShaderName = "debug_unlit";
        desc.vertexResources = {.numUniformBuffers = 1};
        desc.fragmentResources = {.numUniformBuffers = 1};
        desc.vertexLayout = VertexLayouts::PosColour;
        desc.primitiveType = PrimitiveType::LineList;
        desc.cullMode = CullMode::None;
        desc.depthTestEnabled = false;
        desc.depthWriteEnabled = false;

        m_debugLinePipeline = m_context->GetPipelines().GetOrCreate(m_context->GetShaders(), desc);
        if (!m_debugLinePipeline.IsValid())
        {
            WAYFINDER_WARN(LogRenderer, "DebugPass: Failed to create debug line pipeline");
        }
    }

    void DebugPass::OnDetach(const RenderFeatureContext& /*context*/)
    {
        m_debugLinePipeline = {};
        m_context = nullptr;
    }

    void DebugPass::AddPasses(RenderGraph& graph, const FrameRenderParams& params)
    {
        if (!m_context)
        {
            WAYFINDER_WARN(LogRenderer, "DebugPass: no context — skipped");
            return;
        }

        // Overlay must write the same colour target `CompositionPass` will sample (post chain), not `SceneColour`.
        const RenderGraphHandle colourHandle = ResolvePostProcessInput(graph);

        // ── Pre-compute debug draw data during setup ─────────
        // Collect unique view indices, build vertex data into scratch buffers
        // (retained across frames to avoid heap allocation), and upload via
        // TransientBufferAllocator. The execute lambda only draws.
        auto& transientAllocator = m_context->GetTransientBuffers();

        std::array<PerViewDebugDraw, MAX_DEBUG_VIEWS> viewDraws{};
        uint32_t viewCount = 0;
        m_scratchBoxes.clear();

        // Identify unique views and build per-view line vertices + box ranges
        for (const auto& layer : params.Frame.Layers)
        {
            if (!layer.Enabled || !layer.DebugDraw)
            {
                continue;
            }

            const size_t viewIdx = layer.ViewIndex;

            // Find existing view slot or create one
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
        }

        // Build line vertices and boxes per view, upload lines immediately
        for (uint32_t v = 0; v < viewCount; ++v)
        {
            auto& vd = viewDraws[v];
            m_scratchLines.clear();

            for (const auto& layer : params.Frame.Layers)
            {
                if (!layer.Enabled || !layer.DebugDraw || layer.ViewIndex != vd.ViewIndex)
                {
                    continue;
                }

                if (layer.DebugDraw->ShowWorldGrid)
                {
                    AppendWorldGridLineVertices(m_scratchLines, {.Slices = layer.DebugDraw->WorldGridSlices, .Spacing = layer.DebugDraw->WorldGridSpacing});
                }

                for (const auto& line : layer.DebugDraw->Lines)
                {
                    const Float3 lineColour = LinearColour::FromColour(line.Tint).ToFloat3();
                    m_scratchLines.push_back({.Position = line.Start, .Colour = lineColour});
                    m_scratchLines.push_back({.Position = line.End, .Colour = lineColour});
                }

                if (vd.BoxCount == 0)
                {
                    vd.BoxStart = static_cast<uint32_t>(m_scratchBoxes.size());
                }

                // NOLINTNEXTLINE(bugprone-unchecked-optional-access) — we check DebugDraw above
                m_scratchBoxes.insert(m_scratchBoxes.end(), layer.DebugDraw->Boxes.begin(), layer.DebugDraw->Boxes.end());
                vd.BoxCount = static_cast<uint32_t>(m_scratchBoxes.size()) - vd.BoxStart;
            }

            vd.LineVertexCount = static_cast<uint32_t>(m_scratchLines.size());
            if (!m_scratchLines.empty())
            {
                const auto dataSize = static_cast<uint32_t>(m_scratchLines.size() * sizeof(VertexPosColour));
                vd.LineAlloc = transientAllocator.AllocateVertices(m_scratchLines.data(), dataSize);
            }
        }

        // Snapshot box data — the execute lambda references it via pointer+count.
        // m_scratchBoxes persists until the next AddPasses call which is next frame.
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

                auto& debugLinePipeline = m_debugLinePipeline;
                auto& registry = m_context->GetPrograms();
                const Mesh* primitiveMeshPtr = params.BuiltInMeshes[static_cast<size_t>(BuiltInMeshId::PrimitiveColour)];

                // ── Draw lines ───────────────────────────────
                for (uint32_t v = 0; v < viewCount; ++v)
                {
                    const auto& vd = viewDraws[v];
                    if (vd.LineVertexCount == 0 || !vd.LineAlloc.IsValid() || !debugLinePipeline.IsValid())
                    {
                        continue;
                    }

                    const ResolvedViewForLayer resolved = ResolveViewMatricesForLayer(params, vd.ViewIndex);
                    if (!resolved.Ok)
                    {
                        continue;
                    }

                    const UnlitTransformUBO transformUBO{.Mvp = resolved.Proj * resolved.View};
                    const DebugMaterialUBO materialUBO{.BaseColour = Float4(1.0f)};

                    device.BindPipeline(debugLinePipeline);
                    device.BindVertexBuffer(vd.LineAlloc.Buffer, {.offsetInBytes = vd.LineAlloc.Offset});
                    device.PushVertexUniform(0, &transformUBO, sizeof(UnlitTransformUBO));
                    device.PushFragmentUniform(0, &materialUBO, sizeof(DebugMaterialUBO));
                    device.DrawPrimitives(vd.LineVertexCount);
                }

                // ── Draw boxes ───────────────────────────────
                const ShaderProgram* unlitProgram = registry.Find("unlit");
                if (!unlitProgram || !unlitProgram->Pipeline.IsValid() || !primitiveMeshPtr || !primitiveMeshPtr->IsValid())
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

                device.BindPipeline(unlitProgram->Pipeline);
                primitiveMeshPtr->Bind(device);

                for (uint32_t v = 0; v < viewCount; ++v)
                {
                    const auto& vd = viewDraws[v];
                    if (vd.BoxCount == 0)
                    {
                        continue;
                    }

                    const ResolvedViewForLayer resolved = ResolveViewMatricesForLayer(params, vd.ViewIndex);
                    if (!resolved.Ok)
                    {
                        continue;
                    }

                    for (uint32_t b = 0; b < vd.BoxCount; ++b)
                    {
                        const auto& box = boxData[vd.BoxStart + b]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                        const UnlitTransformUBO transformUBO{.Mvp = resolved.Proj * resolved.View * box.LocalToWorld};
                        const DebugMaterialUBO materialUBO{.BaseColour = Float4(1.0f)};

                        device.PushVertexUniform(0, &transformUBO, sizeof(UnlitTransformUBO));
                        device.PushFragmentUniform(0, &materialUBO, sizeof(DebugMaterialUBO));
                        primitiveMeshPtr->Draw(device);
                    }
                }
            };
        });
    }

#ifdef WAYFINDER_COMPILER_MSVC
#pragma warning(pop)
#endif

} // namespace Wayfinder
