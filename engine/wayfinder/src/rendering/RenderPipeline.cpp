#include "RenderPipeline.h"

#include "GPUPipeline.h"
#include "Mesh.h"
#include "RenderContext.h"
#include "RenderDevice.h"
#include "RenderFeature.h"
#include "RenderGraph.h"
#include "RenderResources.h"
#include "ShaderProgram.h"
#include "TransientBufferAllocator.h"
#include "VertexFormats.h"

#include "../core/Log.h"

#include <algorithm>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Wayfinder
{
    namespace
    {
        // Vertex UBO shared by all scene shaders: MVP + Model (128 bytes)
        struct TransformUBO
        {
            Matrix4 mvp;
            Matrix4 model;
        };

        // Unlit shaders only need MVP (64 bytes)
        struct UnlitTransformUBO
        {
            Matrix4 mvp;
        };

        // Fragment material UBO for the debug pipeline (16 bytes)
        struct DebugMaterialUBO
        {
            Float4 baseColor;
        };
    }

    void RenderPipeline::Initialise(RenderContext& context)
    {
        m_context = &context;

        auto& registry = context.GetPrograms();

        // ── Register built-in shader programs ────────────────
        {
            ShaderProgramDesc desc;
            desc.Name = "unlit";
            desc.VertexShaderName = "unlit";
            desc.FragmentShaderName = "unlit";
            desc.VertexResources = {.numUniformBuffers = 1};
            desc.FragmentResources = {.numUniformBuffers = 1};
            desc.VertexLayout = VertexLayouts::PosNormalColor;
            desc.Cull = CullMode::Back;
            desc.DepthTest = true;
            desc.DepthWrite = true;
            desc.MaterialParams = {
                {"base_color", MaterialParamType::Color, 0, LinearColor::White()},
            };
            desc.MaterialUBOSize = 16; // float4
            desc.VertexUBOSize = sizeof(UnlitTransformUBO);
            desc.NeedsSceneGlobals = false;

            registry.Register(desc);
        }

        {
            ShaderProgramDesc desc;
            desc.Name = "basic_lit";
            desc.VertexShaderName = "basic_lit";
            desc.FragmentShaderName = "basic_lit";
            desc.VertexResources = {.numUniformBuffers = 1};
            desc.FragmentResources = {.numUniformBuffers = 2}; // material + scene globals
            desc.VertexLayout = VertexLayouts::PosNormalColor;
            desc.Cull = CullMode::Back;
            desc.DepthTest = true;
            desc.DepthWrite = true;
            desc.MaterialParams = {
                {"base_color", MaterialParamType::Color, 0, LinearColor::White()},
            };
            desc.MaterialUBOSize = 16; // float4
            desc.VertexUBOSize = sizeof(TransformUBO);
            desc.NeedsSceneGlobals = true;

            registry.Register(desc);
        }

        {
            ShaderProgramDesc desc;
            desc.Name = "composition";
            desc.VertexShaderName = "fullscreen";
            desc.FragmentShaderName = "composition";
            desc.VertexResources = {};
            desc.FragmentResources = {.numUniformBuffers = 0, .numSamplers = 1};
            desc.VertexLayout = VertexLayouts::Empty;
            desc.Cull = CullMode::None;
            desc.DepthTest = false;
            desc.DepthWrite = false;
            desc.MaterialUBOSize = 0;
            desc.VertexUBOSize = 0;
            desc.NeedsSceneGlobals = false;

            registry.Register(desc);
        }
    }

    void RenderPipeline::Shutdown()
    {
        m_context = nullptr;
    }

    bool RenderPipeline::Prepare(RenderFrame& frame) const
    {
        if (frame.Views.empty())
        {
            WAYFINDER_WARNING(LogRenderer, "RenderPipeline: frame '{}' has no views — skipped", frame.SceneName);
            return false;
        }

        if (frame.Passes.empty())
        {
            WAYFINDER_WARNING(LogRenderer, "RenderPipeline: frame '{}' has no passes — skipped", frame.SceneName);
            return false;
        }

        for (RenderPass& pass : frame.Passes)
        {
            if (!pass.Enabled || pass.Id.IsEmpty())
            {
                continue;
            }

            if (pass.ViewIndex >= frame.Views.size())
            {
                WAYFINDER_WARNING(LogRenderer, "RenderPipeline: pass '{}' references invalid view index {}", pass.Id, pass.ViewIndex);
                pass.Enabled = false;
                continue;
            }

            // Sort scene pass submissions by sort key (front-to-back for opaque)
            if (pass.Kind == RenderPassKind::Scene)
            {
                std::sort(pass.Meshes.begin(), pass.Meshes.end(),
                    [](const RenderMeshSubmission& a, const RenderMeshSubmission& b)
                    {
                        return a.SortKey < b.SortKey;
                    });
            }
        }

        return true;
    }

    void RenderPipeline::BuildGraph(RenderGraph& graph, const RenderPipelineFrameParams& params) const
    {
        const auto& preparedFrame = params.Frame;
        const uint32_t swapW = params.SwapchainWidth;
        const uint32_t swapH = params.SwapchainHeight;

        // ── Camera / Projection (from primary view) ──────────
        Color clearColor = Color::White();
        Matrix4 view = glm::mat4(1.0f);
        Matrix4 projection = glm::mat4(1.0f);
        bool hasCamera = false;

        if (!preparedFrame.Views.empty() && swapW > 0 && swapH > 0)
        {
            clearColor = preparedFrame.Views.front().ClearColor;
            const auto& camera = preparedFrame.Views.front().CameraState;
            const float aspect = static_cast<float>(swapW) / static_cast<float>(swapH);

            view = glm::lookAt(camera.Position, camera.Target, camera.Up);
            if (camera.ProjectionType == 0)
            {
                projection = glm::perspectiveRH_ZO(glm::radians(camera.FOV), aspect, 0.1f, 1000.0f);
            }
            else
            {
                const float halfH = camera.FOV * 0.5f;
                const float halfW = halfH * aspect;
                projection = glm::orthoRH_ZO(-halfW, halfW, -halfH, halfH, 0.1f, 1000.0f);
            }
            hasCamera = true;
        }

        const SceneGlobalsUBO sceneGlobals = BuildSceneGlobals(preparedFrame);

        // Transient texture descriptions for well-known targets
        RenderGraphTextureDesc colorDesc;
        colorDesc.Width = swapW;
        colorDesc.Height = swapH;
        colorDesc.Format = TextureFormat::RGBA8_UNORM;
        colorDesc.DebugName = WellKnown::SceneColor;

        RenderGraphTextureDesc depthDesc;
        depthDesc.Width = swapW;
        depthDesc.Height = swapH;
        depthDesc.Format = TextureFormat::D32_FLOAT;
        depthDesc.DebugName = WellKnown::SceneDepth;

        // ── MainScene Pass ───────────────────────────────────
        graph.AddPass("MainScene", [&, viewMat = view, projMat = projection, hasCamera](RenderGraphBuilder& builder) -> RenderGraphExecuteFn {
            auto color = builder.CreateTransient(colorDesc);
            auto depth = builder.CreateTransient(depthDesc);
            builder.WriteColor(color, LoadOp::Clear, ClearValue::FromColor(clearColor));
            builder.WriteDepth(depth, LoadOp::Clear, 1.0f);

            return [this, &preparedFrame, &params, viewMat, projMat, sceneGlobals, hasCamera]
                   (RenderDevice& device, const RenderGraphResources& /*resources*/) {
                if (!hasCamera) return;

                const auto& primitiveMesh = params.PrimitiveMesh;
                auto& registry = m_context->GetPrograms();
                const ShaderProgram* lastBoundProgram = nullptr;

                /// Scratch buffer for material UBO data — reused across draws to avoid
                /// per-submission heap allocations.
                std::vector<uint8_t> materialUBOData;

                for (const auto& pass : preparedFrame.Passes)
                {
                    if (!pass.Enabled || pass.Kind != RenderPassKind::Scene) continue;

                    // Per-pass camera from the pass's own view
                    Matrix4 passView = viewMat;
                    Matrix4 passProj = projMat;

                    if (pass.ViewIndex < preparedFrame.Views.size())
                    {
                        const auto& pv = preparedFrame.Views[pass.ViewIndex];
                        const auto& cam = pv.CameraState;
                        const float aspect = (params.SwapchainHeight > 0)
                            ? static_cast<float>(params.SwapchainWidth) / static_cast<float>(params.SwapchainHeight)
                            : 1.0f;

                        passView = glm::lookAt(cam.Position, cam.Target, cam.Up);
                        if (cam.ProjectionType == 0)
                        {
                            passProj = glm::perspectiveRH_ZO(glm::radians(cam.FOV), aspect, 0.1f, 1000.0f);
                        }
                        else
                        {
                            const float halfH = cam.FOV * 0.5f;
                            const float halfW = halfH * aspect;
                            passProj = glm::orthoRH_ZO(-halfW, halfW, -halfH, halfH, 0.1f, 1000.0f);
                        }
                    }

                    for (const auto& submission : pass.Meshes)
                    {
                        if (!submission.Visible) continue;

                        const ShaderProgram* program = registry.FindOrDefault(submission.Material.ShaderName);
                        if (!program || !program->Pipeline) continue;

                        if (program != lastBoundProgram)
                        {
                            program->Pipeline->Bind();
                            primitiveMesh.Bind(device);
                            lastBoundProgram = program;
                        }

                        if (program->Desc.NeedsSceneGlobals)
                        {
                            TransformUBO transformUBO;
                            transformUBO.mvp = passProj * passView * submission.LocalToWorld;
                            transformUBO.model = submission.LocalToWorld;
                            device.PushVertexUniform(0, &transformUBO, sizeof(TransformUBO));
                        }
                        else
                        {
                            UnlitTransformUBO transformUBO{passProj * passView * submission.LocalToWorld};
                            device.PushVertexUniform(0, &transformUBO, sizeof(UnlitTransformUBO));
                        }

                        materialUBOData.assign(program->Desc.MaterialUBOSize, 0);

                        MaterialParameterBlock mergedParams = submission.Material.Parameters;
                        if (submission.Material.HasOverrides)
                        {
                            for (const auto& [name, value] : submission.Material.Overrides.Values)
                            {
                                mergedParams.Values[name] = value;
                            }
                        }

                        mergedParams.SerializeToUBO(program->Desc.MaterialParams,
                                                     materialUBOData.data(),
                                                     static_cast<uint32_t>(materialUBOData.size()));
                        device.PushFragmentUniform(0, materialUBOData.data(),
                                                    static_cast<uint32_t>(materialUBOData.size()));

                        if (program->Desc.NeedsSceneGlobals)
                        {
                            device.PushFragmentUniform(1, &sceneGlobals, sizeof(SceneGlobalsUBO));
                        }

                        primitiveMesh.Draw(device);
                    }
                }
            };
        });

        // ── Debug Pass ───────────────────────────────────────
        graph.AddPass("Debug", [&, viewMat = view, projMat = projection, hasCamera](RenderGraphBuilder& builder) -> RenderGraphExecuteFn {
            auto color = graph.FindHandle(WellKnown::SceneColor);
            auto depth = graph.FindHandle(WellKnown::SceneDepth);
            builder.WriteColor(color, LoadOp::Load);
            builder.WriteDepth(depth, LoadOp::Load);

            return [this, &preparedFrame, &params, viewMat, projMat, hasCamera]
                   (RenderDevice& device, const RenderGraphResources& /*resources*/) {
                if (!hasCamera) return;

                auto& debugLinePipeline = params.DebugLinePipeline;
                const auto& primitiveMesh = params.PrimitiveMesh;
                auto& transientAllocator = m_context->GetTransientBuffers();
                auto& registry = m_context->GetPrograms();

                // ── Debug lines ──────────────────────────────
                std::vector<VertexPosColor> lineVertices;

                for (const auto& pass : preparedFrame.Passes)
                {
                    if (!pass.Enabled || !pass.DebugDraw) continue;

                    if (pass.DebugDraw->ShowWorldGrid)
                    {
                        const int slices = std::max(1, pass.DebugDraw->WorldGridSlices);
                        const float spacing = pass.DebugDraw->WorldGridSpacing;
                        const float extent = static_cast<float>(slices) * spacing;
                        const Float3 majorColor{0.45f, 0.45f, 0.45f};
                        const Float3 minorColor{0.25f, 0.25f, 0.25f};

                        for (int i = -slices; i <= slices; ++i)
                        {
                            const float coord = static_cast<float>(i) * spacing;
                            const Float3& gridColor = (i == 0) ? majorColor : minorColor;

                            lineVertices.push_back({Float3{-extent, 0.0f, coord}, gridColor});
                            lineVertices.push_back({Float3{ extent, 0.0f, coord}, gridColor});
                            lineVertices.push_back({Float3{coord, 0.0f, -extent}, gridColor});
                            lineVertices.push_back({Float3{coord, 0.0f,  extent}, gridColor});
                        }
                    }

                    for (const auto& line : pass.DebugDraw->Lines)
                    {
                        const Float3 lineColor = LinearColor::FromColor(line.Color).ToFloat3();
                        lineVertices.push_back({line.Start, lineColor});
                        lineVertices.push_back({line.End, lineColor});
                    }
                }

                if (!lineVertices.empty() && debugLinePipeline.IsValid())
                {
                    const uint32_t dataSize = static_cast<uint32_t>(lineVertices.size() * sizeof(VertexPosColor));
                    const TransientAllocation alloc = transientAllocator.AllocateVertices(lineVertices.data(), dataSize);

                    if (alloc.IsValid())
                    {
                        const Matrix4 mvp = projMat * viewMat;
                        const DebugMaterialUBO materialUBO{glm::vec4(1.0f)};

                        debugLinePipeline.Bind();
                        device.BindVertexBuffer(alloc.Buffer, 0, alloc.Offset);
                        device.PushVertexUniform(0, &mvp, sizeof(Matrix4));
                        device.PushFragmentUniform(0, &materialUBO, sizeof(DebugMaterialUBO));
                        device.DrawPrimitives(static_cast<uint32_t>(lineVertices.size()));
                    }
                }

                // ── Debug boxes ──────────────────────────────
                const ShaderProgram* unlitProgram = registry.Find("unlit");
                if (!unlitProgram || !unlitProgram->Pipeline || !primitiveMesh.IsValid()) return;

                bool hasPendingBoxes = false;
                for (const auto& pass : preparedFrame.Passes)
                {
                    if (pass.Enabled && pass.DebugDraw && !pass.DebugDraw->Boxes.empty())
                    {
                        hasPendingBoxes = true;
                        break;
                    }
                }

                if (!hasPendingBoxes) return;

                unlitProgram->Pipeline->Bind();
                primitiveMesh.Bind(device);

                for (const auto& pass : preparedFrame.Passes)
                {
                    if (!pass.Enabled || !pass.DebugDraw) continue;

                    for (const auto& box : pass.DebugDraw->Boxes)
                    {
                        const UnlitTransformUBO transformUBO{projMat * viewMat * box.LocalToWorld};
                        const DebugMaterialUBO materialUBO{glm::vec4(1.0f)};

                        device.PushVertexUniform(0, &transformUBO, sizeof(UnlitTransformUBO));
                        device.PushFragmentUniform(0, &materialUBO, sizeof(DebugMaterialUBO));
                        primitiveMesh.Draw(device);
                    }
                }
            };
        });

        // ── Feature passes ───────────────────────────────────
        for (const auto& feature : params.Features)
        {
            if (feature->IsEnabled())
            {
                feature->AddPasses(graph, preparedFrame);
            }
        }

        // ── Composition Pass ─────────────────────────────────
        graph.AddPass("Composition", [&](RenderGraphBuilder& builder) -> RenderGraphExecuteFn {
            auto color = graph.FindHandle(WellKnown::SceneColor);
            builder.ReadTexture(color);
            builder.SetSwapchainOutput(LoadOp::DontCare);

            return [this, color](RenderDevice& device, const RenderGraphResources& resources) {
                auto sceneColorTex = resources.GetTexture(color);

                const ShaderProgram* compProgram = m_context->GetPrograms().Find("composition");
                const auto nearestSampler = m_context->GetNearestSampler();
                if (!compProgram || !compProgram->Pipeline || !sceneColorTex || !nearestSampler) return;

                compProgram->Pipeline->Bind();
                device.BindFragmentSampler(0, sceneColorTex, nearestSampler);
                device.DrawPrimitives(3);
            };
        });
    }

    SceneGlobalsUBO RenderPipeline::BuildSceneGlobals(const RenderFrame& frame) const
    {
        SceneGlobalsUBO globals;

        for (const auto& light : frame.Lights)
        {
            if (light.Type == RenderLightType::Directional)
            {
                globals.LightDirection = glm::normalize(light.Direction);
                globals.LightIntensity = light.Intensity;
                globals.LightColor = LinearColor::FromColor(light.Tint).ToFloat3();
                return globals;
            }
        }

        // Default: sun-like light from upper-right
        globals.LightDirection = glm::normalize(Float3{-0.4f, -0.7f, -0.5f});
        return globals;
    }

} // namespace Wayfinder