#include "DebugPass.h"

#include "rendering/backend/RenderDevice.h"
#include "rendering/backend/VertexFormats.h"
#include "rendering/graph/RenderFrameUtils.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/graph/RenderPassCapabilities.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/pipeline/RenderContext.h"
#include "rendering/pipeline/ShaderUniforms.h"
#include "rendering/resources/TransientBufferAllocator.h"

#include "core/Log.h"
#include "maths/Maths.h"

#ifdef WAYFINDER_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4324)
#endif

namespace Wayfinder
{
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

        const PreparedPrimaryView& primary = params.PrimaryView;
        const Matrix4 view = primary.ViewMatrix;
        const Matrix4 projection = primary.ProjectionMatrix;
        const bool hasCamera = primary.Valid;

        graph.AddPass("Debug", [&, viewMat = view, projMat = projection, hasCamera](RenderGraphBuilder& builder)
        {
            builder.DeclarePassCapabilities(RenderPassCapabilities::Raster | RenderPassCapabilities::RasterOverlayOrDebug);
            auto colour = graph.FindHandleChecked(GraphTextureId::SceneColour);
            auto depth = graph.FindHandleChecked(GraphTextureId::SceneDepth);
            builder.WriteColour(colour, LoadOp::Load);
            builder.WriteDepth(depth, LoadOp::Load);

            return [this, &params, viewMat, projMat, hasCamera](RenderDevice& device, const RenderGraphResources& /*resources*/)
            {
                if (!hasCamera || !m_context)
                {
                    return;
                }

                auto& debugLinePipeline = m_debugLinePipeline;
                auto& transientAllocator = m_context->GetTransientBuffers();
                auto& registry = m_context->GetPrograms();

                const auto debugMeshIt = params.MeshesByStride.find(VertexLayouts::PosNormalColour.stride);
                const Mesh* primitiveMeshPtr = (debugMeshIt != params.MeshesByStride.end()) ? debugMeshIt->second : nullptr;

                std::vector<VertexPosColour> lineVertices;

                for (const auto& layer : params.Frame.Layers)
                {
                    if (!layer.Enabled || !layer.DebugDraw)
                    {
                        continue;
                    }

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
                }

                if (!lineVertices.empty() && debugLinePipeline.IsValid())
                {
                    const auto dataSize = static_cast<uint32_t>(lineVertices.size() * sizeof(VertexPosColour));
                    const TransientAllocation alloc = transientAllocator.AllocateVertices(lineVertices.data(), dataSize);

                    if (alloc.IsValid())
                    {
                        const UnlitTransformUBO transformUBO{.Mvp = projMat * viewMat};
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
                for (const auto& layer : params.Frame.Layers)
                {
                    if (layer.Enabled && layer.DebugDraw && !layer.DebugDraw->Boxes.empty())
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

                for (const auto& layer : params.Frame.Layers)
                {
                    if (!layer.Enabled || !layer.DebugDraw)
                    {
                        continue;
                    }

                    for (const auto& box : layer.DebugDraw->Boxes)
                    {
                        const UnlitTransformUBO transformUBO{.Mvp = projMat * viewMat * box.LocalToWorld};
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
