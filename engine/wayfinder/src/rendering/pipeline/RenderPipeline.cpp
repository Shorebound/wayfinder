#include "RenderPipeline.h"

#include "rendering/backend/GPUPipeline.h"
#include "rendering/mesh/Mesh.h"
#include "PipelineCache.h"
#include "RenderContext.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/graph/RenderFeature.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/resources/RenderResources.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/resources/TransientBufferAllocator.h"
#include "rendering/backend/VertexFormats.h"

#include "core/Log.h"
#include "maths/Maths.h"

#include <algorithm>
#include <vector>

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
            Float4 baseColour;
        };

        /// Builds a PipelineCreateDesc for the wireframe variant of a shader program.
        /// Returns std::nullopt if the shader handles cannot be resolved.
        std::optional<PipelineCreateDesc> MakeWireframeVariant(
            const ShaderProgramDesc& desc, ShaderManager& shaders)
        {
            GPUShaderHandle vs = shaders.GetShader(desc.VertexShaderName, ShaderStage::Vertex, desc.VertexResources);
            GPUShaderHandle fs = shaders.GetShader(desc.FragmentShaderName, ShaderStage::Fragment, desc.FragmentResources);
            if (!vs || !fs) return std::nullopt;

            PipelineCreateDesc pipeDesc{};
            pipeDesc.vertexShader = vs;
            pipeDesc.fragmentShader = fs;
            pipeDesc.vertexLayout = desc.VertexLayout;
            pipeDesc.primitiveType = PrimitiveType::TriangleList;
            pipeDesc.cullMode = desc.Cull;
            pipeDesc.fillMode = FillMode::Line;
            pipeDesc.frontFace = FrontFace::CounterClockwise;
            pipeDesc.depthTestEnabled = desc.DepthTest;
            pipeDesc.depthWriteEnabled = desc.DepthWrite;
            return pipeDesc;
        }
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
            desc.VertexLayout = VertexLayouts::PosNormalColour;
            desc.Cull = CullMode::Back;
            desc.DepthTest = true;
            desc.DepthWrite = true;
            desc.MaterialParams = {
                {"base_colour", MaterialParamType::Colour, 0, LinearColour::White()},
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
            desc.VertexLayout = VertexLayouts::PosNormalColour;
            desc.Cull = CullMode::Back;
            desc.DepthTest = true;
            desc.DepthWrite = true;
            desc.MaterialParams = {
                {"base_colour", MaterialParamType::Colour, 0, LinearColour::White()},
            };
            desc.MaterialUBOSize = 16; // float4
            desc.VertexUBOSize = sizeof(TransformUBO);
            desc.NeedsSceneGlobals = true;

            registry.Register(desc);
        }

        {
            ShaderProgramDesc desc;
            desc.Name = "textured_lit";
            desc.VertexShaderName = "textured_lit";
            desc.FragmentShaderName = "textured_lit";
            desc.VertexResources = {.numUniformBuffers = 1};
            desc.FragmentResources = {.numUniformBuffers = 2, .numSamplers = 1}; // material + scene globals + diffuse sampler
            desc.VertexLayout = VertexLayouts::PosNormalUV;
            desc.Cull = CullMode::Back;
            desc.DepthTest = true;
            desc.DepthWrite = true;
            desc.MaterialParams = {
                {"base_colour", MaterialParamType::Colour, 0, LinearColour::White()},
            };
            desc.MaterialUBOSize = 16; // float4
            desc.VertexUBOSize = sizeof(TransformUBO);
            desc.NeedsSceneGlobals = true;
            desc.TextureSlots = {{"diffuse", 0}};

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
        Colour clearColour = Colour::White();
        Matrix4 view = Matrix4(1.0f);
        Matrix4 projection = Matrix4(1.0f);
        bool hasCamera = false;

        if (!preparedFrame.Views.empty() && swapW > 0 && swapH > 0)
        {
            clearColour = preparedFrame.Views.front().ClearColour;
            const auto& camera = preparedFrame.Views.front().CameraState;
            const float aspect = static_cast<float>(swapW) / static_cast<float>(swapH);

            view = Maths::LookAt(camera.Position, camera.Target, camera.Up);
            if (camera.ProjectionType == 0)
            {
                projection = Maths::PerspectiveRH_ZO(Maths::ToRadians(camera.FOV), aspect, 0.1f, 1000.0f);
            }
            else
            {
                const float halfH = camera.FOV * 0.5f;
                const float halfW = halfH * aspect;
                projection = Maths::OrthoRH_ZO(-halfW, halfW, -halfH, halfH, 0.1f, 1000.0f);
            }
            hasCamera = true;
        }

        const SceneGlobalsUBO sceneGlobals = BuildSceneGlobals(preparedFrame);

        // Transient texture descriptions for well-known targets
        RenderGraphTextureDesc colourDesc;
        colourDesc.Width = swapW;
        colourDesc.Height = swapH;
        colourDesc.Format = TextureFormat::RGBA8_UNORM;
        colourDesc.DebugName = WellKnown::SceneColour;

        RenderGraphTextureDesc depthDesc;
        depthDesc.Width = swapW;
        depthDesc.Height = swapH;
        depthDesc.Format = TextureFormat::D32_FLOAT;
        depthDesc.DebugName = WellKnown::SceneDepth;

        // ── MainScene Pass ───────────────────────────────────
#ifdef WAYFINDER_COMPILER_MSVC
    #pragma warning(push)
    #pragma warning(disable : 4324) // lambda closure padded due to captured alignas(16) matrices
#endif
        graph.AddPass("MainScene", [&, viewMat = view, projMat = projection, hasCamera](RenderGraphBuilder& builder) {
            auto colour = builder.CreateTransient(colourDesc);
            auto depth = builder.CreateTransient(depthDesc);
            builder.WriteColour(colour, LoadOp::Clear, ClearValue::FromColour(clearColour));
            builder.WriteDepth(depth, LoadOp::Clear, 1.0f);

            return [this, &preparedFrame, &params, viewMat, projMat, sceneGlobals, hasCamera]
                   (RenderDevice& device, const RenderGraphResources& /*resources*/) {
                if (!hasCamera) return;

                auto& registry = m_context->GetPrograms();
                auto& pipelineCache = m_context->GetPipelines();
                auto& shaderManager = m_context->GetShaders();
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

                        passView = Maths::LookAt(cam.Position, cam.Target, cam.Up);
                        if (cam.ProjectionType == 0)
                        {
                            passProj = Maths::PerspectiveRH_ZO(Maths::ToRadians(cam.FOV), aspect, 0.1f, 1000.0f);
                        }
                        else
                        {
                            const float halfH = cam.FOV * 0.5f;
                            const float halfW = halfH * aspect;
                            passProj = Maths::OrthoRH_ZO(-halfW, halfW, -halfH, halfH, 0.1f, 1000.0f);
                        }
                    }

                    for (const auto& submission : pass.Meshes)
                    {
                        if (!submission.Visible) continue;

                        const ShaderProgram* program = registry.FindOrDefault(submission.Material.ShaderName);
                        if (!program || !program->Pipeline) continue;

                        // Determine fill mode: use override if set, otherwise default solid
                        const RenderFillMode fillMode = submission.Material.StateOverrides.FillMode
                            .value_or(RenderFillMode::Solid);

                        // Helper: push transform + material uniforms for this submission
                        auto pushUniforms = [&]()
                        {
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

                            mergedParams.SerialiseToUBO(program->Desc.MaterialParams,
                                                          materialUBOData.data(),
                                                          static_cast<uint32_t>(materialUBOData.size()));
                            device.PushFragmentUniform(0, materialUBOData.data(),
                                                         static_cast<uint32_t>(materialUBOData.size()));

                            if (program->Desc.NeedsSceneGlobals)
                            {
                                device.PushFragmentUniform(1, &sceneGlobals, sizeof(SceneGlobalsUBO));
                            }
                        };

                        // Solid draw (for Solid and SolidAndWireframe modes)
                        if (fillMode == RenderFillMode::Solid || fillMode == RenderFillMode::SolidAndWireframe)
                        {
                            // Look up the mesh for this program's vertex layout
                            const auto meshIt = params.MeshesByStride.find(program->Desc.VertexLayout.stride);
                            if (meshIt == params.MeshesByStride.end() || !meshIt->second)
                            {
                                continue;
                            }
                            const auto& mesh = *meshIt->second;

                            if (program != lastBoundProgram)
                            {
                                program->Pipeline->Bind();
                                mesh.Bind(device);
                                lastBoundProgram = program;
                            }

                            pushUniforms();

                            // Bind resolved textures for this submission
                            for (const auto& texBinding : submission.Material.ResolvedTextures)
                            {
                                if (texBinding.Texture && texBinding.Sampler)
                                {
                                    device.BindFragmentSampler(texBinding.Slot, texBinding.Texture, texBinding.Sampler);
                                }
                            }

                            mesh.Draw(device);
                        }

                        // Wireframe draw (for Wireframe and SolidAndWireframe modes)
                        if (fillMode == RenderFillMode::Wireframe || fillMode == RenderFillMode::SolidAndWireframe)
                        {
                            const auto wireframeDesc = MakeWireframeVariant(program->Desc, shaderManager);
                            if (wireframeDesc)
                            {
                                GPUPipelineHandle wireframePipeline = pipelineCache.GetOrCreate(*wireframeDesc);
                                if (wireframePipeline.IsValid())
                                {
                                    // Select the mesh matching the program's vertex layout
                                    const auto wireframeMeshIt = params.MeshesByStride.find(program->Desc.VertexLayout.stride);
                                    if (wireframeMeshIt != params.MeshesByStride.end() && wireframeMeshIt->second)
                                    {
                                        auto& wireframeMesh = *wireframeMeshIt->second;
                                        device.BindPipeline(wireframePipeline);
                                        wireframeMesh.Bind(device);
                                        lastBoundProgram = nullptr; // Force re-bind on next solid draw

                                        // Bind textures so textured wireframe shaders have valid samplers
                                        for (const auto& texBinding : submission.Material.ResolvedTextures)
                                        {
                                            if (texBinding.Texture && texBinding.Sampler)
                                            {
                                                device.BindFragmentSampler(texBinding.Slot, texBinding.Texture, texBinding.Sampler);
                                            }
                                        }

                                        pushUniforms();
                                        wireframeMesh.Draw(device);
                                    }
                                }
                            }
                        }
                    }
                }
            };
        });

        // ── Debug Pass ───────────────────────────────────────
        graph.AddPass("Debug", [&, viewMat = view, projMat = projection, hasCamera](RenderGraphBuilder& builder) {
            auto colour = graph.FindHandle(WellKnown::SceneColour);
            auto depth = graph.FindHandle(WellKnown::SceneDepth);
            builder.WriteColour(colour, LoadOp::Load);
            builder.WriteDepth(depth, LoadOp::Load);

            return [this, &preparedFrame, &params, viewMat, projMat, hasCamera]
                   (RenderDevice& device, const RenderGraphResources& /*resources*/) {
                if (!hasCamera) return;

                auto& debugLinePipeline = params.DebugLinePipeline;
                auto& transientAllocator = m_context->GetTransientBuffers();
                auto& registry = m_context->GetPrograms();

                // Resolve PosNormalColour mesh for debug boxes
                const auto debugMeshIt = params.MeshesByStride.find(VertexLayouts::PosNormalColour.stride);
                Mesh* primitiveMeshPtr = (debugMeshIt != params.MeshesByStride.end()) ? debugMeshIt->second : nullptr;

                // ── Debug lines ──────────────────────────────
                std::vector<VertexPosColour> lineVertices;

                for (const auto& pass : preparedFrame.Passes)
                {
                    if (!pass.Enabled || !pass.DebugDraw) continue;

                    if (pass.DebugDraw->ShowWorldGrid)
                    {
                        const int slices = std::max(1, pass.DebugDraw->WorldGridSlices);
                        const float spacing = pass.DebugDraw->WorldGridSpacing;
                        const float extent = static_cast<float>(slices) * spacing;
                        const Float3 majorColour{0.45f, 0.45f, 0.45f};
                        const Float3 minorColour{0.25f, 0.25f, 0.25f};

                        for (int i = -slices; i <= slices; ++i)
                        {
                            const float coord = static_cast<float>(i) * spacing;
                            const Float3& gridColour = (i == 0) ? majorColour : minorColour;

                            lineVertices.push_back({Float3{-extent, 0.0f, coord}, gridColour});
                            lineVertices.push_back({Float3{ extent, 0.0f, coord}, gridColour});
                            lineVertices.push_back({Float3{coord, 0.0f, -extent}, gridColour});
                            lineVertices.push_back({Float3{coord, 0.0f,  extent}, gridColour});
                        }
                    }

                    for (const auto& line : pass.DebugDraw->Lines)
                    {
                        const Float3 lineColour = LinearColour::FromColour(line.Tint).ToFloat3();
                        lineVertices.push_back({line.Start, lineColour});
                        lineVertices.push_back({line.End, lineColour});
                    }
                }

                if (!lineVertices.empty() && debugLinePipeline.IsValid())
                {
                    const uint32_t dataSize = static_cast<uint32_t>(lineVertices.size() * sizeof(VertexPosColour));
                    const TransientAllocation alloc = transientAllocator.AllocateVertices(lineVertices.data(), dataSize);

                    if (alloc.IsValid())
                    {
                        const Matrix4 mvp = projMat * viewMat;
                        const DebugMaterialUBO materialUBO{Float4(1.0f)};

                        debugLinePipeline.Bind();
                        device.BindVertexBuffer(alloc.Buffer, 0, alloc.Offset);
                        device.PushVertexUniform(0, &mvp, sizeof(Matrix4));
                        device.PushFragmentUniform(0, &materialUBO, sizeof(DebugMaterialUBO));
                        device.DrawPrimitives(static_cast<uint32_t>(lineVertices.size()));
                    }
                }

                // ── Debug boxes ──────────────────────────────
                const ShaderProgram* unlitProgram = registry.Find("unlit");
                if (!unlitProgram || !unlitProgram->Pipeline || !primitiveMeshPtr || !primitiveMeshPtr->IsValid()) return;

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
                primitiveMeshPtr->Bind(device);

                for (const auto& pass : preparedFrame.Passes)
                {
                    if (!pass.Enabled || !pass.DebugDraw) continue;

                    for (const auto& box : pass.DebugDraw->Boxes)
                    {
                        const UnlitTransformUBO transformUBO{projMat * viewMat * box.LocalToWorld};
                        const DebugMaterialUBO materialUBO{Float4(1.0f)};

                        device.PushVertexUniform(0, &transformUBO, sizeof(UnlitTransformUBO));
                        device.PushFragmentUniform(0, &materialUBO, sizeof(DebugMaterialUBO));
                        primitiveMeshPtr->Draw(device);
                    }
                }
            };
        });
#ifdef WAYFINDER_COMPILER_MSVC
    #pragma warning(pop)
#endif

        // ── Feature passes ───────────────────────────────────
        for (const auto& feature : params.Features)
        {
            if (feature->IsEnabled())
            {
                feature->AddPasses(graph, preparedFrame);
            }
        }

        // ── Composition Pass ─────────────────────────────────
        graph.AddPass("Composition", [&](RenderGraphBuilder& builder) {
            auto colour = graph.FindHandle(WellKnown::SceneColour);
            builder.ReadTexture(colour);
            builder.SetSwapchainOutput(LoadOp::DontCare);

            return [this, colour](RenderDevice& device, const RenderGraphResources& resources) {
                auto sceneColourTex = resources.GetTexture(colour);

                const ShaderProgram* compProgram = m_context->GetPrograms().Find("composition");
                const auto nearestSampler = m_context->GetNearestSampler();
                if (!compProgram || !compProgram->Pipeline || !sceneColourTex || !nearestSampler) return;

                compProgram->Pipeline->Bind();
                device.BindFragmentSampler(0, sceneColourTex, nearestSampler);
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
                globals.LightDirection = Maths::Normalize(light.Direction);
                globals.LightIntensity = light.Intensity;
                globals.LightColour = LinearColour::FromColour(light.Tint).ToFloat3();
                return globals;
            }
        }

        // Default: sun-like light from upper-right
        globals.LightDirection = Maths::Normalize(Float3{-0.4f, -0.7f, -0.5f});
        return globals;
    }

} // namespace Wayfinder
