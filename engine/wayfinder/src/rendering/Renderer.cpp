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

        glm::vec4 ColorToVec4(const Color& c)
        {
            return {
                static_cast<float>(c.r) / 255.0f,
                static_cast<float>(c.g) / 255.0f,
                static_cast<float>(c.b) / 255.0f,
                static_cast<float>(c.a) / 255.0f,
            };
        }

        // Fragment UBO for the unlit shader (16 bytes)
        struct UnlitMaterialUBO
        {
            glm::vec4 baseColor;
        };

        // Vertex UBO for the basic_lit shader (128 bytes: two 4×4 matrices)
        struct BasicLitTransformUBO
        {
            Matrix4 mvp;
            Matrix4 model;
        };

        // Fragment UBO for the basic_lit shader (48 bytes)
        struct BasicLitMaterialUBO
        {
            glm::vec4 baseColor;       // 16 bytes
            Float3 lightDirection;     // 12 bytes
            float lightIntensity;      //  4 bytes
            Float3 lightColor;         // 12 bytes
            float ambient;             //  4 bytes
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

        // ── Unlit pipeline ───────────────────────────────────
        {
            GPUPipelineDesc desc{};
            desc.vertexShaderName = "unlit";
            desc.fragmentShaderName = "unlit";
            desc.vertexResources = {.numUniformBuffers = 1};
            desc.fragmentResources = {.numUniformBuffers = 1};
            desc.vertexLayout = VertexLayouts::PosColor;
            desc.cullMode = CullMode::Back;
            desc.depthTestEnabled = true;
            desc.depthWriteEnabled = true;

            auto& pipeline = m_pipelines["unlit"];
            if (!pipeline.Create(device, m_shaderManager, desc, &m_pipelineCache))
            {
                WAYFINDER_WARNING(LogRenderer, "Renderer: Failed to create unlit pipeline");
            }
        }

        // ── Basic lit pipeline ───────────────────────────────
        {
            GPUPipelineDesc desc{};
            desc.vertexShaderName = "basic_lit";
            desc.fragmentShaderName = "basic_lit";
            desc.vertexResources = {.numUniformBuffers = 1};
            desc.fragmentResources = {.numUniformBuffers = 1};
            desc.vertexLayout = VertexLayouts::PosNormalColor;
            desc.cullMode = CullMode::Back;
            desc.depthTestEnabled = true;
            desc.depthWriteEnabled = true;

            auto& pipeline = m_pipelines["basic_lit"];
            if (!pipeline.Create(device, m_shaderManager, desc, &m_pipelineCache))
            {
                WAYFINDER_WARNING(LogRenderer, "Renderer: Failed to create basic_lit pipeline");
            }
        }

        // ── Debug line pipeline ──────────────────────────────
        {
            GPUPipelineDesc desc{};
            desc.vertexShaderName = "unlit";
            desc.fragmentShaderName = "unlit";
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

        // Create built-in geometry
        m_cubeMesh = Mesh::CreateUnitCube(device);
        m_litCubeMesh = Mesh::CreateUnitCubeWithNormals(device);

        WAYFINDER_INFO(LogRenderer, "Renderer initialized ({}x{}, backend: {})",
            m_screenWidth, m_screenHeight, device.GetDeviceInfo().BackendName);

        return true;
    }

    void Renderer::Shutdown()
    {
        m_transientAllocator.Shutdown();
        m_litCubeMesh.Destroy();
        m_cubeMesh.Destroy();
        m_debugLinePipeline.Destroy();

        for (auto& [name, pipeline] : m_pipelines)
        {
            pipeline.Destroy();
        }
        m_pipelines.clear();

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

    GPUPipeline* Renderer::GetPipelineForShader(const std::string& shaderName)
    {
        auto it = m_pipelines.find(shaderName);
        if (it != m_pipelines.end() && it->second.IsValid())
        {
            return &it->second;
        }

        // Fallback to unlit
        it = m_pipelines.find("unlit");
        if (it != m_pipelines.end() && it->second.IsValid())
        {
            return &it->second;
        }

        return nullptr;
    }

    Mesh* Renderer::GetMeshForShader(const std::string& shaderName)
    {
        if (shaderName == "basic_lit" && m_litCubeMesh.IsValid())
        {
            return &m_litCubeMesh;
        }
        return &m_cubeMesh;
    }

    void Renderer::ExtractPrimaryLight(const RenderFrame& frame, Float3& direction, float& intensity, Float3& color) const
    {
        // Default: sun-like light from upper-right
        direction = glm::normalize(Float3{-0.4f, -0.7f, -0.5f});
        intensity = 1.0f;
        color = Float3{1.0f, 1.0f, 1.0f};

        for (const auto& light : frame.Lights)
        {
            if (light.Type == RenderLightType::Directional)
            {
                direction = glm::normalize(light.Direction);
                intensity = light.Intensity;
                color = ColorToFloat3(light.Tint);
                return;
            }
        }
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
                const UnlitMaterialUBO materialUBO{glm::vec4(1.0f)}; // white pass-through

                m_debugLinePipeline.Bind();
                m_device->BindVertexBuffer(alloc.Buffer, 0, alloc.Offset);
                m_device->PushVertexUniform(0, &mvp, sizeof(Matrix4));
                m_device->PushFragmentUniform(0, &materialUBO, sizeof(UnlitMaterialUBO));
                m_device->DrawPrimitives(static_cast<uint32_t>(lineVertices.size()));
            }
        }

        // ── Draw debug boxes (reuse unit cube mesh) ──────────
        GPUPipeline* unlitPipeline = GetPipelineForShader("unlit");
        if (!unlitPipeline || !m_cubeMesh.IsValid())
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

        unlitPipeline->Bind();
        m_cubeMesh.Bind(*m_device);

        for (const auto& pass : frame.Passes)
        {
            if (!pass.Enabled || !pass.DebugDraw)
            {
                continue;
            }

            for (const auto& box : pass.DebugDraw->Boxes)
            {
                const Matrix4 mvp = projection * view * box.LocalToWorld;
                const UnlitMaterialUBO materialUBO{glm::vec4(1.0f)};

                m_device->PushVertexUniform(0, &mvp, sizeof(Matrix4));
                m_device->PushFragmentUniform(0, &materialUBO, sizeof(UnlitMaterialUBO));
                m_cubeMesh.Draw(*m_device);
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

            // Extract primary directional light for basic_lit
            Float3 lightDir;
            float lightIntensity;
            Float3 lightColor;
            ExtractPrimaryLight(preparedFrame, lightDir, lightIntensity, lightColor);

            // ── Scene passes — per-material pipeline binding ─
            // Sort all scene pass submissions by sort key
            for (auto& pass : preparedFrame.Passes)
            {
                if (pass.Enabled && pass.Kind == RenderPassKind::Scene)
                {
                    std::sort(pass.Meshes.begin(), pass.Meshes.end(),
                        [](const RenderMeshSubmission& a, const RenderMeshSubmission& b)
                        {
                            return a.SortKey < b.SortKey;
                        });
                }
            }

            std::string lastBoundShader;

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

                    const std::string& shaderName = submission.Material.ShaderName;

                    // Bind pipeline + mesh if shader changed
                    if (shaderName != lastBoundShader)
                    {
                        GPUPipeline* pipeline = GetPipelineForShader(shaderName);
                        Mesh* mesh = GetMeshForShader(shaderName);

                        if (!pipeline || !mesh || !mesh->IsValid())
                        {
                            continue;
                        }

                        pipeline->Bind();
                        mesh->Bind(*m_device);
                        lastBoundShader = shaderName;
                    }

                    // Push uniforms and draw based on shader type
                    if (shaderName == "basic_lit")
                    {
                        BasicLitTransformUBO transformUBO;
                        transformUBO.mvp = projection * view * submission.LocalToWorld;
                        transformUBO.model = submission.LocalToWorld;
                        m_device->PushVertexUniform(0, &transformUBO, sizeof(BasicLitTransformUBO));

                        BasicLitMaterialUBO materialUBO;
                        materialUBO.baseColor = ColorToVec4(submission.Material.BaseColor);
                        materialUBO.lightDirection = lightDir;
                        materialUBO.lightIntensity = lightIntensity;
                        materialUBO.lightColor = lightColor;
                        materialUBO.ambient = 0.15f;
                        m_device->PushFragmentUniform(0, &materialUBO, sizeof(BasicLitMaterialUBO));

                        GetMeshForShader(shaderName)->Draw(*m_device);
                    }
                    else
                    {
                        // Unlit path (default)
                        const Matrix4 mvp = projection * view * submission.LocalToWorld;
                        m_device->PushVertexUniform(0, &mvp, sizeof(Matrix4));

                        UnlitMaterialUBO materialUBO;
                        materialUBO.baseColor = ColorToVec4(submission.Material.BaseColor);
                        m_device->PushFragmentUniform(0, &materialUBO, sizeof(UnlitMaterialUBO));

                        GetMeshForShader(shaderName)->Draw(*m_device);
                    }
                }
            }

            // ── Debug passes (grid, lines, debug boxes) ──────
            RenderDebugPass(preparedFrame, view, projection);
        }

        m_device->EndRenderPass();
        m_device->EndFrame();
    }

} // namespace Wayfinder
