#include "Renderer.h"
#include "RenderFrame.h"
#include "RenderPipeline.h"
#include "RenderResources.h"

#include "../core/ServiceLocator.h"
#include "../rendering/RenderAPI.h"
#include "../rendering/GraphicsContext.h"

namespace Wayfinder
{
    Renderer::Renderer()
        : m_screenWidth(800), m_screenHeight(450), m_isInitialized(false)
    {
        m_clearColor = Color::White();
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

    bool Renderer::Initialize(int screenWidth, int screenHeight)
    {
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;

        const auto& renderCapabilities = ServiceLocator::GetRenderAPI().GetCapabilities();
        const auto& contextCapabilities = ServiceLocator::GetGraphicsContext().GetCapabilities();

        if (renderCapabilities.BackendName != contextCapabilities.BackendName)
        {
            return false;
        }

        // Additional renderer initialization can go here
        // For example, loading shaders, creating render targets, etc.

        m_isInitialized = true;
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
        if (!m_isInitialized)
            return;

        RenderFrame preparedFrame = frame;
        const auto& capabilities = ServiceLocator::GetRenderAPI().GetCapabilities();

        if (capabilities.MaxViewCount == 0)
        {
            return;
        }

        if (!preparedFrame.Views.empty())
        {
            m_clearColor = preparedFrame.Views.front().ClearColor;
        }

        BeginFrame();

        if (m_renderResources)
        {
            m_renderResources->PrepareFrame(preparedFrame);
        }

        if (m_renderPipeline && m_renderResources)
        {
            m_renderPipeline->Execute(preparedFrame, ServiceLocator::GetRenderAPI(), *m_renderResources);
        }

        // Get render API from service locator
        auto& renderAPI = ServiceLocator::GetRenderAPI();

        // Draw scene info
        renderAPI.DrawText("Scene: " + preparedFrame.SceneName, 10, 30, 20, Color::DarkGray());

        EndFrame();
    }

    void Renderer::BeginFrame()
    {
        auto& graphicsContext = ServiceLocator::GetGraphicsContext();
        //auto& renderAPI = ServiceLocator::GetRenderAPI();

        graphicsContext.BeginFrame();
        graphicsContext.Clear(
            static_cast<float>(m_clearColor.r) / 255.0f,
            static_cast<float>(m_clearColor.g) / 255.0f,
            static_cast<float>(m_clearColor.b) / 255.0f,
            static_cast<float>(m_clearColor.a) / 255.0f);
    }

    void Renderer::EndFrame()
    {
        auto& renderAPI = ServiceLocator::GetRenderAPI();
        auto& graphicsContext = ServiceLocator::GetGraphicsContext();

        renderAPI.DrawFPS(10, 10);
        graphicsContext.EndFrame();
    }

} // namespace Wayfinder
