#include "DebugPass.h"

#include "rendering/backend/RenderDevice.h"
#include "rendering/backend/VertexFormats.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/graph/RenderPassCapabilities.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/pipeline/RenderContext.h"
#include "rendering/pipeline/ShaderUniforms.h"
#include "rendering/resources/TransientBufferAllocator.h"

#include "core/Log.h"
#include "maths/Maths.h"

#include <unordered_map>

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
        ResolvedViewForLayer ResolveViewMatricesForLayer(const RenderPipelineFrameParams& params, size_t viewIndex)
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

    void DebugPass::OnAttach(const RenderPassContext& context)
    {
        m_context = &context.Context;
        auto& device = m_context->GetDevice();

        GPUPipelineDesc desc{};
        desc.vertexShaderName = "debug_unlit";
        desc.fragmentShaderName = "unlit";
        desc.vertexResources = {.numUniformBuffers = 1};
        desc.fragmentResources = {.numUniformBuffers = 1};
        desc.vertexLayout = VertexLayouts::PosColour;
        desc.primitiveType = PrimitiveType::LineList;
        desc.cullMode = CullMode::None;
        desc.depthTestEnabled = false;
        desc.depthWriteEnabled = false;

        if (!m_debugLinePipeline.Create(device, m_context->GetShaders(), desc, &m_context->GetPipelines()))
        {
            WAYFINDER_WARN(LogRenderer, "DebugPass: Failed to create debug line pipeline");
        }
    }

    void DebugPass::OnDetach(const RenderPassContext& /*context*/)
    {
        m_debugLinePipeline.Destroy();
        m_context = nullptr;
    }

    void DebugPass::AddPasses(RenderGraph& graph, const RenderPipelineFrameParams& params)
    {
        if (!m_context)
        {
            WAYFINDER_WARN(LogRenderer, "DebugPass: no context — skipped");
            return;
        }

        graph.AddPass("Debug", [&](RenderGraphBuilder& builder)
        {
            builder.DeclarePassCapabilities(RenderPassCapabilities::RASTER | RenderPassCapabilities::RASTER_OVERLAY_OR_DEBUG);
            auto colour = graph.FindHandleChecked(GraphTextureId::SceneColour);
            auto depth = graph.FindHandleChecked(GraphTextureId::SceneDepth);
            builder.WriteColour(colour, LoadOp::Load);
            builder.WriteDepth(depth, LoadOp::Load);

            return [this, &params](RenderDevice& device, const RenderGraphResources& /*resources*/)
            {
                if (!m_context)
                {
                    return;
                }

                auto& debugLinePipeline = m_debugLinePipeline;
                auto& transientAllocator = m_context->GetTransientBuffers();
                auto& registry = m_context->GetPrograms();

                const auto debugMeshIt = params.MeshesByStride.find(VertexLayouts::PosNormalColour.stride);
                const Mesh* primitiveMeshPtr = (debugMeshIt != params.MeshesByStride.end()) ? debugMeshIt->second : nullptr;

                std::unordered_map<size_t, std::vector<VertexPosColour>> lineBuckets;
                std::unordered_map<size_t, std::vector<RenderDebugBox>> boxBuckets;

                for (const auto& layer : params.Frame.Layers)
                {
                    if (!layer.Enabled || !layer.DebugDraw)
                    {
                        continue;
                    }

                    const size_t viewIdx = layer.ViewIndex;
                    std::vector<VertexPosColour>& lineVertices = lineBuckets[viewIdx];

                    if (layer.DebugDraw->ShowWorldGrid)
                    {
                        const int slices = std::max(1, layer.DebugDraw->WorldGridSlices);
                        const float spacing = layer.DebugDraw->WorldGridSpacing;
                        const float extent = static_cast<float>(slices) * spacing;
                        const Float3 majorColour{0.45f, 0.45f, 0.45f};
                        const Float3 minorColour{0.25f, 0.25f, 0.25f};

                        for (int i = -slices; i <= slices; ++i)
                        {
                            const float coord = static_cast<float>(i) * spacing;
                            const Float3& gridColour = (i == 0) ? majorColour : minorColour;

                            lineVertices.push_back({.Position = Float3{-extent, 0.0f, coord}, .Colour = gridColour});
                            lineVertices.push_back({.Position = Float3{extent, 0.0f, coord}, .Colour = gridColour});
                            lineVertices.push_back({.Position = Float3{coord, 0.0f, -extent}, .Colour = gridColour});
                            lineVertices.push_back({.Position = Float3{coord, 0.0f, extent}, .Colour = gridColour});
                        }
                    }

                    for (const auto& line : layer.DebugDraw->Lines)
                    {
                        const Float3 lineColour = LinearColour::FromColour(line.Tint).ToFloat3();
                        lineVertices.push_back({.Position = line.Start, .Colour = lineColour});
                        lineVertices.push_back({.Position = line.End, .Colour = lineColour});
                    }

                    if (!layer.DebugDraw->Boxes.empty())
                    {
                        auto& boxes = boxBuckets[viewIdx];
                        boxes.insert(boxes.end(), layer.DebugDraw->Boxes.begin(), layer.DebugDraw->Boxes.end());
                    }
                }

                for (const auto& [viewIndex, lineVertices] : lineBuckets)
                {
                    if (lineVertices.empty() || !debugLinePipeline.IsValid())
                    {
                        continue;
                    }

                    const ResolvedViewForLayer resolved = ResolveViewMatricesForLayer(params, viewIndex);
                    if (!resolved.Ok)
                    {
                        continue;
                    }

                    const auto dataSize = static_cast<uint32_t>(lineVertices.size() * sizeof(VertexPosColour));
                    const TransientAllocation alloc = transientAllocator.AllocateVertices(lineVertices.data(), dataSize);

                    if (alloc.IsValid())
                    {
                        const UnlitTransformUBO transformUBO{.Mvp = resolved.Proj * resolved.View};
                        const DebugMaterialUBO materialUBO{.BaseColour = Float4(1.0f)};

                        debugLinePipeline.Bind();
                        device.BindVertexBuffer(alloc.Buffer, {.offsetInBytes = alloc.Offset});
                        device.PushVertexUniform(0, &transformUBO, sizeof(UnlitTransformUBO));
                        device.PushFragmentUniform(0, &materialUBO, sizeof(DebugMaterialUBO));
                        device.DrawPrimitives(static_cast<uint32_t>(lineVertices.size()));
                    }
                }

                const ShaderProgram* unlitProgram = registry.Find("unlit");
                if (!unlitProgram || !unlitProgram->Pipeline || !primitiveMeshPtr || !primitiveMeshPtr->IsValid())
                {
                    return;
                }

                bool hasPendingBoxes = false;
                for (const auto& entry : boxBuckets)
                {
                    if (!entry.second.empty())
                    {
                        hasPendingBoxes = true;
                        break;
                    }
                }

                if (!hasPendingBoxes)
                {
                    return;
                }

                unlitProgram->Pipeline->Bind();
                primitiveMeshPtr->Bind(device);

                for (const auto& [viewIndex, boxes] : boxBuckets)
                {
                    if (boxes.empty())
                    {
                        continue;
                    }

                    const ResolvedViewForLayer resolved = ResolveViewMatricesForLayer(params, viewIndex);
                    if (!resolved.Ok)
                    {
                        continue;
                    }

                    for (const auto& box : boxes)
                    {
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
