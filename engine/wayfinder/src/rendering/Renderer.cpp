#include "Renderer.h"

#include "RenderDevice.h"
#include "RenderFrame.h"
#include "RenderPipeline.h"
#include "RenderResources.h"

#include "../core/EngineConfig.h"
#include "../core/Log.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Wayfinder
{
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

        // Create built-in geometry
        m_cubeMesh = Mesh::CreateUnitCube(device);

        WAYFINDER_INFO(LogRenderer, "Renderer initialized ({}x{}, backend: {})",
            m_screenWidth, m_screenHeight, device.GetDeviceInfo().BackendName);

        return true;
    }

    void Renderer::Shutdown()
    {
        m_cubeMesh.Destroy();
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

        if (m_unlitPipeline.IsValid() && m_cubeMesh.IsValid() && !preparedFrame.Views.empty())
        {
            m_unlitPipeline.Bind();

            const auto& camera = preparedFrame.Views.front().CameraState;
            const float aspect = static_cast<float>(m_screenWidth) / static_cast<float>(m_screenHeight);

            const Matrix4 view = glm::lookAt(camera.Position, camera.Target, camera.Up);

            Matrix4 projection;
            if (camera.ProjectionType == 0) // Perspective
            {
                projection = glm::perspective(glm::radians(camera.FOV), aspect, 0.1f, 1000.0f);
            }
            else // Orthographic
            {
                const float halfH = camera.FOV * 0.5f;
                const float halfW = halfH * aspect;
                projection = glm::ortho(-halfW, halfW, -halfH, halfH, 0.1f, 1000.0f);
            }

            m_cubeMesh.Bind(*m_device);

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

        m_device->EndRenderPass();
        m_device->EndFrame();
    }

} // namespace Wayfinder
