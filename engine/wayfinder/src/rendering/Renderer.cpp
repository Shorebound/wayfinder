#include "Renderer.h"

#include "RenderDevice.h"
#include "RenderFrame.h"
#include "RenderPipeline.h"
#include "RenderResources.h"
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
        Float3 ColorToFloat3(const Color& c)
        {
            return {
                static_cast<float>(c.r) / 255.0f,
                static_cast<float>(c.g) / 255.0f,
                static_cast<float>(c.b) / 255.0f,
            };
        }

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

        // Single built-in mesh for all scene primitives
        m_primitiveMesh = Mesh::CreatePrimitive(device);

        WAYFINDER_INFO(LogRenderer, "Renderer initialized ({}x{}, backend: {})",
            m_screenWidth, m_screenHeight, device.GetDeviceInfo().BackendName);

        return true;
    }

    void Renderer::Shutdown()
    {
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
                globals.LightColor = ColorToFloat3(light.Tint);
                return globals;
            }
        }

        // Default: sun-like light from upper-right
        globals.LightDirection = glm::normalize(Float3{-0.4f, -0.7f, -0.5f});
        return globals;
    }

    void Renderer::RenderDebugPass(const RenderFrame& frame, const Matrix4& view, const Matrix4& projection)
    {
        if (!m_device)
        {
            return;
        }

        // ── Collect debug line vertices ──────────────────────
        std::vector<VertexPosColor> lineVertices;

        for (const auto& pass : frame.Passes)
        {
            if (!pass.Enabled || !pass.DebugDraw)
            {
                continue;
            }

            // World grid
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

            // Explicit debug lines
            for (const auto& line : pass.DebugDraw->Lines)
            {
                const Float3 lineColor = ColorToFloat3(line.Color);
                lineVertices.push_back({line.Start, lineColor});
                lineVertices.push_back({line.End, lineColor});
            }
        }

        // ── Draw debug lines via transient allocator ─────────
        if (!lineVertices.empty() && m_debugLinePipeline.IsValid())
        {
            const uint32_t dataSize = static_cast<uint32_t>(lineVertices.size() * sizeof(VertexPosColor));
            const TransientAllocation alloc = m_transientAllocator.AllocateVertices(lineVertices.data(), dataSize);

            if (alloc.IsValid())
            {
                const Matrix4 mvp = projection * view;
                const DebugMaterialUBO materialUBO{glm::vec4(1.0f)}; // white pass-through

                m_debugLinePipeline.Bind();
                m_device->BindVertexBuffer(alloc.Buffer, 0, alloc.Offset);
                m_device->PushVertexUniform(0, &mvp, sizeof(Matrix4));
                m_device->PushFragmentUniform(0, &materialUBO, sizeof(DebugMaterialUBO));
                m_device->DrawPrimitives(static_cast<uint32_t>(lineVertices.size()));
            }
        }

        // ── Draw debug boxes (reuse primitive mesh with unlit program) ──
        const ShaderProgram* unlitProgram = m_programRegistry.Find("unlit");
        if (!unlitProgram || !unlitProgram->Pipeline || !m_primitiveMesh.IsValid())
        {
            return;
        }

        bool hasPendingBoxes = false;
        for (const auto& pass : frame.Passes)
        {
            if (pass.Enabled && pass.DebugDraw && !pass.DebugDraw->Boxes.empty())
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
        m_primitiveMesh.Bind(*m_device);

        for (const auto& pass : frame.Passes)
        {
            if (!pass.Enabled || !pass.DebugDraw)
            {
                continue;
            }

            for (const auto& box : pass.DebugDraw->Boxes)
            {
                const UnlitTransformUBO transformUBO{projection * view * box.LocalToWorld};
                const DebugMaterialUBO materialUBO{glm::vec4(1.0f)};

                m_device->PushVertexUniform(0, &transformUBO, sizeof(UnlitTransformUBO));
                m_device->PushFragmentUniform(0, &materialUBO, sizeof(DebugMaterialUBO));
                m_primitiveMesh.Draw(*m_device);
            }
        }
    }

    void Renderer::Render(const RenderFrame& frame)
    {
        if (!m_isInitialized || !m_device)
        {
            return;
        }

        if (!m_device->BeginFrame())
        {
            return;
        }

        m_transientAllocator.BeginFrame();

        // Resolve material bindings from asset data
        RenderFrame preparedFrame = frame;
        if (m_renderResources)
        {
            m_renderResources->PrepareFrame(preparedFrame);
        }

        // Determine clear color from the primary view
        Color clearColor = Color::White();
        if (!preparedFrame.Views.empty())
        {
            clearColor = preparedFrame.Views.front().ClearColor;
        }

        // Main render pass — clears the swapchain and presents
        RenderPassDescriptor passDesc;
        passDesc.debugName = "MainSwapchain";
        passDesc.colorAttachment.clearValue = ClearValue::FromColor(clearColor);
        passDesc.colorAttachment.loadOp = LoadOp::Clear;
        passDesc.colorAttachment.storeOp = StoreOp::Store;
        passDesc.targetSwapchain = true;
        passDesc.depthAttachment.enabled = true;
        passDesc.depthAttachment.clearDepth = 1.0f;
        passDesc.depthAttachment.loadOp = LoadOp::Clear;
        passDesc.depthAttachment.storeOp = StoreOp::DontCare;

        m_device->BeginRenderPass(passDesc);

        if (!preparedFrame.Views.empty())
        {
            const auto& camera = preparedFrame.Views.front().CameraState;
            const float aspect = static_cast<float>(m_screenWidth) / static_cast<float>(m_screenHeight);

            const Matrix4 view = glm::lookAt(camera.Position, camera.Target, camera.Up);

            // SDL_GPU / Vulkan clip space: Z range [0, 1]. Use _ZO (zero-to-one) variants.
            Matrix4 projection;
            if (camera.ProjectionType == 0) // Perspective
            {
                projection = glm::perspectiveRH_ZO(glm::radians(camera.FOV), aspect, 0.1f, 1000.0f);
            }
            else // Orthographic
            {
                const float halfH = camera.FOV * 0.5f;
                const float halfW = halfH * aspect;
                projection = glm::orthoRH_ZO(-halfW, halfW, -halfH, halfH, 0.1f, 1000.0f);
            }

            // Build per-frame scene globals from lights
            const SceneGlobalsUBO sceneGlobals = BuildSceneGlobals(preparedFrame);

            // ── Validate and sort passes via RenderPipeline ──
            if (!m_renderPipeline->Prepare(preparedFrame))
            {
                m_device->EndRenderPass();
                m_device->EndFrame();
                return;
            }

            const ShaderProgram* lastBoundProgram = nullptr;

            for (const auto& pass : preparedFrame.Passes)
            {
                if (!pass.Enabled || pass.Kind != RenderPassKind::Scene)
                {
                    continue;
                }

                for (const auto& submission : pass.Meshes)
                {
                    if (!submission.Visible)
                    {
                        continue;
                    }

                    const ShaderProgram* program = m_programRegistry.FindOrDefault(submission.Material.ShaderName);
                    if (!program || !program->Pipeline)
                    {
                        continue;
                    }

                    // Bind pipeline + mesh when program changes
                    if (program != lastBoundProgram)
                    {
                        program->Pipeline->Bind();
                        m_primitiveMesh.Bind(*m_device);
                        lastBoundProgram = program;
                    }

                    // Push vertex transform UBO
                    if (program->Desc.NeedsSceneGlobals)
                    {
                        // basic_lit: needs MVP + Model for normal transformation
                        TransformUBO transformUBO;
                        transformUBO.mvp = projection * view * submission.LocalToWorld;
                        transformUBO.model = submission.LocalToWorld;
                        m_device->PushVertexUniform(0, &transformUBO, sizeof(TransformUBO));
                    }
                    else
                    {
                        // unlit: only needs MVP
                        UnlitTransformUBO transformUBO{projection * view * submission.LocalToWorld};
                        m_device->PushVertexUniform(0, &transformUBO, sizeof(UnlitTransformUBO));
                    }

                    // Serialize material parameters into UBO bytes
                    std::vector<uint8_t> materialUBOData(program->Desc.MaterialUBOSize, 0);

                    // Merge: start from asset-loaded parameters, apply per-entity overrides on top
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
                    m_device->PushFragmentUniform(0, materialUBOData.data(),
                                                   static_cast<uint32_t>(materialUBOData.size()));

                    // Push scene globals to fragment slot 1 if needed
                    if (program->Desc.NeedsSceneGlobals)
                    {
                        m_device->PushFragmentUniform(1, &sceneGlobals, sizeof(SceneGlobalsUBO));
                    }

                    m_primitiveMesh.Draw(*m_device);
                }
            }

            // ── Debug passes (grid, lines, debug boxes) ──────
            RenderDebugPass(preparedFrame, view, projection);
        }

        m_device->EndRenderPass();
        m_device->EndFrame();
    }

} // namespace Wayfinder
