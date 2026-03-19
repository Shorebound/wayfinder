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

        // Create the unlit pipeline
        GPUPipelineDesc unlitDesc{};
        unlitDesc.vertexShaderName = "unlit";
        unlitDesc.fragmentShaderName = "unlit";
        unlitDesc.vertexLayout = VertexLayouts::PosColor;
        unlitDesc.cullMode = CullMode::Back;
        unlitDesc.depthTestEnabled = false;
        unlitDesc.depthWriteEnabled = false;

        if (!m_unlitPipeline.Create(device, m_shaderManager, unlitDesc))
        {
            WAYFINDER_WARNING(LogRenderer, "Renderer: Failed to create unlit pipeline");
        }

        // Create the debug line pipeline (same shader, line topology, no culling)
        GPUPipelineDesc debugLineDesc{};
        debugLineDesc.vertexShaderName = "unlit";
        debugLineDesc.fragmentShaderName = "unlit";
        debugLineDesc.vertexLayout = VertexLayouts::PosColor;
        debugLineDesc.primitiveType = PrimitiveType::LineList;
        debugLineDesc.cullMode = CullMode::None;
        debugLineDesc.depthTestEnabled = false;
        debugLineDesc.depthWriteEnabled = false;

        if (!m_debugLinePipeline.Create(device, m_shaderManager, debugLineDesc))
        {
            WAYFINDER_WARNING(LogRenderer, "Renderer: Failed to create debug line pipeline");
        }

        // Transient allocator: 4 MB vertex ring, 1 MB index ring
        if (!m_transientAllocator.Initialize(device, 4u * 1024u * 1024u, 1u * 1024u * 1024u))
        {
            WAYFINDER_WARNING(LogRenderer, "Renderer: Failed to initialize transient buffer allocator");
        }

        // Create built-in geometry
        m_cubeMesh = Mesh::CreateUnitCube(device);

        WAYFINDER_INFO(LogRenderer, "Renderer initialized ({}x{}, backend: {})",
            m_screenWidth, m_screenHeight, device.GetDeviceInfo().BackendName);

        return true;
    }

    void Renderer::Shutdown()
    {
        m_transientAllocator.Shutdown();
        m_cubeMesh.Destroy();
        m_debugLinePipeline.Destroy();
        m_unlitPipeline.Destroy();
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
                    const Float3& color = (i == 0) ? majorColor : minorColor;

                    lineVertices.push_back({Float3{-extent, 0.0f, coord}, color});
                    lineVertices.push_back({Float3{ extent, 0.0f, coord}, color});
                    lineVertices.push_back({Float3{coord, 0.0f, -extent}, color});
                    lineVertices.push_back({Float3{coord, 0.0f,  extent}, color});
                }
            }

            // Explicit debug lines
            for (const auto& line : pass.DebugDraw->Lines)
            {
                const Float3 color = ColorToFloat3(line.Color);
                lineVertices.push_back({line.Start, color});
                lineVertices.push_back({line.End, color});
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

                m_debugLinePipeline.Bind();
                m_device->BindVertexBuffer(alloc.Buffer, 0, alloc.Offset);
                m_device->PushVertexUniform(0, &mvp, sizeof(Matrix4));
                m_device->DrawPrimitives(static_cast<uint32_t>(lineVertices.size()));
            }
        }

        // ── Draw debug boxes (reuse unit cube mesh) ──────────
        if (!m_unlitPipeline.IsValid() || !m_cubeMesh.IsValid())
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

        m_unlitPipeline.Bind();
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
                m_device->PushVertexUniform(0, &mvp, sizeof(Matrix4));
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

            // ── Scene passes ─────────────────────────────────
            if (m_unlitPipeline.IsValid() && m_cubeMesh.IsValid())
            {
                m_unlitPipeline.Bind();
                m_cubeMesh.Bind(*m_device);

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

                        const Matrix4 mvp = projection * view * submission.LocalToWorld;
                        m_device->PushVertexUniform(0, &mvp, sizeof(Matrix4));
                        m_cubeMesh.Draw(*m_device);
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
