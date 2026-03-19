#include "Renderer.h"

#include "RenderDevice.h"
#include "RenderFeature.h"
#include "RenderFrame.h"
#include "RenderGraph.h"
#include "RenderPipeline.h"
#include "RenderResources.h"
#include "TransientResourcePool.h"
#include "VertexFormats.h"

#include "../core/EngineConfig.h"
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
            glm::vec4 baseColor;
        };
    }

    Renderer::Renderer()
        : m_screenWidth(800), m_screenHeight(450), m_isInitialized(false)
    {
        m_renderPipeline = std::make_unique<RenderPipeline>();
        m_renderResources = std::make_unique<RenderResourceCache>();
    }

    Renderer::~Renderer()
    {
        if (m_isInitialized)
        {
            Shutdown();
        }
    }

    bool Renderer::Initialize(RenderDevice& device, const EngineConfig& config)
    {
        m_device = &device;
        m_screenWidth = static_cast<int>(config.Window.Width);
        m_screenHeight = static_cast<int>(config.Window.Height);
        m_isInitialized = true;

        m_shaderManager.Initialize(device, config.Shaders.Directory);
        m_pipelineCache.Initialize(device);
        m_programRegistry.Initialize(device, m_shaderManager, m_pipelineCache);

        // ── Register shader programs ─────────────────────────
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

            m_programRegistry.Register(desc);
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

            m_programRegistry.Register(desc);
        }

        // ── Debug line pipeline (PosColor, uses debug_unlit shaders) ──
        {
            GPUPipelineDesc desc{};
            desc.vertexShaderName = "debug_unlit";
            desc.fragmentShaderName = "unlit"; // shares the same fragment shader
            desc.vertexResources = {.numUniformBuffers = 1};
            desc.fragmentResources = {.numUniformBuffers = 1};
            desc.vertexLayout = VertexLayouts::PosColor;
            desc.primitiveType = PrimitiveType::LineList;
            desc.cullMode = CullMode::None;
            desc.depthTestEnabled = false;
            desc.depthWriteEnabled = false;

            if (!m_debugLinePipeline.Create(device, m_shaderManager, desc, &m_pipelineCache))
            {
                WAYFINDER_WARNING(LogRenderer, "Renderer: Failed to create debug line pipeline");
            }
        }

        // Transient allocator: 4 MB vertex ring, 1 MB index ring
        if (!m_transientAllocator.Initialize(device, 4u * 1024u * 1024u, 1u * 1024u * 1024u))
        {
            WAYFINDER_WARNING(LogRenderer, "Renderer: Failed to initialize transient buffer allocator");
        }

        // Transient texture pool for render graph
        m_transientPool.Initialize(device);

        // ── Composition pipeline (fullscreen blit, 1 sampler) ──
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

            m_programRegistry.Register(desc);
        }

        // Nearest-point sampler for composition blit
        {
            SamplerCreateDesc samplerDesc;
            samplerDesc.minFilter = SamplerFilter::Nearest;
            samplerDesc.magFilter = SamplerFilter::Nearest;
            samplerDesc.addressModeU = SamplerAddressMode::ClampToEdge;
            samplerDesc.addressModeV = SamplerAddressMode::ClampToEdge;
            m_nearestSampler = device.CreateSampler(samplerDesc);
        }

        // Single built-in mesh for all scene primitives
        m_primitiveMesh = Mesh::CreatePrimitive(device);

        WAYFINDER_INFO(LogRenderer, "Renderer initialized ({}x{}, backend: {})",
            m_screenWidth, m_screenHeight, device.GetDeviceInfo().BackendName);

        return true;
    }

    void Renderer::Shutdown()
    {
        // Remove features first (they may hold GPU resources)
        for (auto& feature : m_features)
        {
            auto ctx = MakeFeatureContext();
            feature->OnDetach(ctx);
        }
        m_features.clear();

        if (m_nearestSampler)
        {
            m_device->DestroySampler(m_nearestSampler);
            m_nearestSampler = nullptr;
        }

        m_transientPool.Shutdown();
        m_transientAllocator.Shutdown();
        m_primitiveMesh.Destroy();
        m_debugLinePipeline.Destroy();
        m_programRegistry.Shutdown();
        m_pipelineCache.Shutdown();
        m_shaderManager.Shutdown();

        m_renderPipeline = std::make_unique<RenderPipeline>();
        m_renderResources = std::make_unique<RenderResourceCache>();
        if (m_assetService)
        {
            m_renderResources->SetAssetService(m_assetService);
        }
        m_device = nullptr;
        m_isInitialized = false;
    }

    void Renderer::SetAssetService(const std::shared_ptr<AssetService>& assetService)
    {
        m_assetService = assetService;
        if (m_renderResources)
        {
            m_renderResources->SetAssetService(assetService);
        }
    }

    SceneGlobalsUBO Renderer::BuildSceneGlobals(const RenderFrame& frame) const
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

    RenderFeatureContext Renderer::MakeFeatureContext()
    {
        return RenderFeatureContext{
            .Device = *m_device,
            .ProgramRegistry = m_programRegistry,
            .ShaderManager = m_shaderManager,
            .PipelineCache = m_pipelineCache,
            .NearestSampler = m_nearestSampler,
        };
    }

    void Renderer::AddFeature(std::unique_ptr<RenderFeature> feature)
    {
        if (m_device) { auto ctx = MakeFeatureContext(); feature->OnAttach(ctx); }
        m_features.push_back(std::move(feature));
    }



    void Renderer::Render(const RenderFrame& frame)
    {
        if (!m_isInitialized || !m_device) return;
        if (!m_device->BeginFrame()) return;

        m_transientAllocator.BeginFrame();

        // Resolve material bindings from asset data
        RenderFrame preparedFrame = frame;
        if (m_renderResources)
        {
            m_renderResources->PrepareFrame(preparedFrame);
        }

        // Validate and sort passes via RenderPipeline
        if (!m_renderPipeline->Prepare(preparedFrame))
        {
            m_device->EndFrame();
            return;
        }

        // ── Query swapchain dimensions for transient targets ──
        uint32_t swapW = 0, swapH = 0;
        m_device->GetSwapchainDimensions(swapW, swapH);
        if (swapW == 0 || swapH == 0)
        {
            m_device->EndFrame();
            return;
        }

        // ── Camera / Projection (from primary view) ──────────
        Color clearColor = Color::White();
        Matrix4 view = glm::mat4(1.0f);
        Matrix4 projection = glm::mat4(1.0f);
        bool hasCamera = false;

        if (!preparedFrame.Views.empty())
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

        // ── Build Render Graph ───────────────────────────────
        RenderGraph graph;

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
        // Creates transient SceneColor + SceneDepth, renders all scene geometry.
        graph.AddPass("MainScene", [&](RenderGraphBuilder& builder) -> RenderGraphExecuteFn {
            auto color = builder.CreateTransient(colorDesc);
            auto depth = builder.CreateTransient(depthDesc);
            builder.WriteColor(color, LoadOp::Clear, ClearValue::FromColor(clearColor));
            builder.WriteDepth(depth, LoadOp::Clear, 1.0f);

            return [this, &preparedFrame, viewMat = view, projMat = projection, &sceneGlobals, hasCamera]
                   (RenderDevice& device, const RenderGraphResources& /*resources*/) {
                if (!hasCamera) return;

                const ShaderProgram* lastBoundProgram = nullptr;

                for (const auto& pass : preparedFrame.Passes)
                {
                    if (!pass.Enabled || pass.Kind != RenderPassKind::Scene) continue;

                    for (const auto& submission : pass.Meshes)
                    {
                        if (!submission.Visible) continue;

                        const ShaderProgram* program = m_programRegistry.FindOrDefault(submission.Material.ShaderName);
                        if (!program || !program->Pipeline) continue;

                        if (program != lastBoundProgram)
                        {
                            program->Pipeline->Bind();
                            m_primitiveMesh.Bind(device);
                            lastBoundProgram = program;
                        }

                        if (program->Desc.NeedsSceneGlobals)
                        {
                            TransformUBO transformUBO;
                            transformUBO.mvp = projMat * viewMat * submission.LocalToWorld;
                            transformUBO.model = submission.LocalToWorld;
                            device.PushVertexUniform(0, &transformUBO, sizeof(TransformUBO));
                        }
                        else
                        {
                            UnlitTransformUBO transformUBO{projMat * viewMat * submission.LocalToWorld};
                            device.PushVertexUniform(0, &transformUBO, sizeof(UnlitTransformUBO));
                        }

                        std::vector<uint8_t> materialUBOData(program->Desc.MaterialUBOSize, 0);

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

                        m_primitiveMesh.Draw(device);
                    }
                }
            };
        });

        // ── Debug Pass ───────────────────────────────────────
        // Writes on top of SceneColor/Depth with LoadOp::Load.
        graph.AddPass("Debug", [&](RenderGraphBuilder& builder) -> RenderGraphExecuteFn {
            auto color = graph.FindHandle(WellKnown::SceneColor);
            auto depth = graph.FindHandle(WellKnown::SceneDepth);
            builder.WriteColor(color, LoadOp::Load);
            builder.WriteDepth(depth, LoadOp::Load);

            return [this, &preparedFrame, viewMat = view, projMat = projection, hasCamera]
                   (RenderDevice& device, const RenderGraphResources& /*resources*/) {
                if (!hasCamera) return;

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

                if (!lineVertices.empty() && m_debugLinePipeline.IsValid())
                {
                    const uint32_t dataSize = static_cast<uint32_t>(lineVertices.size() * sizeof(VertexPosColor));
                    const TransientAllocation alloc = m_transientAllocator.AllocateVertices(lineVertices.data(), dataSize);

                    if (alloc.IsValid())
                    {
                        const Matrix4 mvp = projMat * viewMat;
                        const DebugMaterialUBO materialUBO{glm::vec4(1.0f)};

                        m_debugLinePipeline.Bind();
                        device.BindVertexBuffer(alloc.Buffer, 0, alloc.Offset);
                        device.PushVertexUniform(0, &mvp, sizeof(Matrix4));
                        device.PushFragmentUniform(0, &materialUBO, sizeof(DebugMaterialUBO));
                        device.DrawPrimitives(static_cast<uint32_t>(lineVertices.size()));
                    }
                }

                // ── Debug boxes ──────────────────────────────
                const ShaderProgram* unlitProgram = m_programRegistry.Find("unlit");
                if (!unlitProgram || !unlitProgram->Pipeline || !m_primitiveMesh.IsValid()) return;

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
                m_primitiveMesh.Bind(device);

                for (const auto& pass : preparedFrame.Passes)
                {
                    if (!pass.Enabled || !pass.DebugDraw) continue;

                    for (const auto& box : pass.DebugDraw->Boxes)
                    {
                        const UnlitTransformUBO transformUBO{projMat * viewMat * box.LocalToWorld};
                        const DebugMaterialUBO materialUBO{glm::vec4(1.0f)};

                        device.PushVertexUniform(0, &transformUBO, sizeof(UnlitTransformUBO));
                        device.PushFragmentUniform(0, &materialUBO, sizeof(DebugMaterialUBO));
                        m_primitiveMesh.Draw(device);
                    }
                }
            };
        });

        // ── Feature passes ───────────────────────────────────
        for (auto& feature : m_features)
        {
            if (feature->IsEnabled())
            {
                feature->AddPasses(graph, preparedFrame);
            }
        }

        // ── Composition Pass ─────────────────────────────────
        // Reads SceneColor, blits to swapchain.
        graph.AddPass("Composition", [&](RenderGraphBuilder& builder) -> RenderGraphExecuteFn {
            auto color = graph.FindHandle(WellKnown::SceneColor);
            builder.ReadTexture(color);
            builder.SetSwapchainOutput(LoadOp::DontCare);

            return [this, color](RenderDevice& device, const RenderGraphResources& resources) {
                auto sceneColorTex = resources.GetTexture(color);

                const ShaderProgram* compProgram = m_programRegistry.Find("composition");
                if (!compProgram || !compProgram->Pipeline || !sceneColorTex || !m_nearestSampler) return;

                compProgram->Pipeline->Bind();
                device.BindFragmentSampler(0, sceneColorTex, m_nearestSampler);
                device.DrawPrimitives(3);
            };
        });

        // ── Compile and Execute ──────────────────────────────
        if (graph.Compile())
        {
            graph.Execute(*m_device, m_transientPool);
        }

        m_device->EndFrame();
    }

} // namespace Wayfinder
