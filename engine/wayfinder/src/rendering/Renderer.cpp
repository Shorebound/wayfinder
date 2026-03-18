#include "Renderer.h"

#include "RenderDevice.h"
#include "RenderFrame.h"
#include "RenderPipeline.h"
#include "RenderResources.h"

#include "../core/Log.h"

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

    bool Renderer::Initialize(RenderDevice& device, int screenWidth, int screenHeight)
    {
        m_device = &device;
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;
        m_isInitialized = true;

        WAYFINDER_INFO(LogRenderer, "Renderer initialized ({}x{}, backend: {})",
            screenWidth, screenHeight, device.GetDeviceInfo().BackendName);

        return true;
    }

    void Renderer::Shutdown()
    {
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

        // Stage 4: m_renderPipeline->Execute(preparedFrame, *m_device, *m_renderResources);

        m_device->EndRenderPass();
        m_device->EndFrame();
    }

} // namespace Wayfinder
