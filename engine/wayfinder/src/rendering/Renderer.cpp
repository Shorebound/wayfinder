#include "Renderer.h"

#include "RenderFrame.h"
#include "RenderPipeline.h"
#include "RenderResources.h"

#include "../rendering/GraphicsContext.h"
#include "../rendering/RenderAPI.h"

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

        if (!m_graphicsContext || !m_renderAPI)
        {
            return false;
        }

        const auto& renderCapabilities = m_renderAPI->GetCapabilities();
        const auto& contextCapabilities = m_graphicsContext->GetCapabilities();
        if (renderCapabilities.BackendName != contextCapabilities.BackendName)
        {
            return false;
        }

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

    void Renderer::SetRenderInterfaces(IGraphicsContext& graphicsContext, IRenderAPI& renderAPI)
    {
        m_graphicsContext = &graphicsContext;
        m_renderAPI = &renderAPI;
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
        {
            return;
        }

        RenderFrame preparedFrame = frame;
        const auto& capabilities = m_renderAPI->GetCapabilities();
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
            m_renderPipeline->Execute(preparedFrame, *m_renderAPI, *m_renderResources);
        }

        m_renderAPI->DrawText("Scene: " + preparedFrame.SceneName, 10, 30, 20, Color::DarkGray());

        EndFrame();
    }

    void Renderer::BeginFrame()
    {
        m_graphicsContext->BeginFrame();
        m_graphicsContext->Clear(
            static_cast<float>(m_clearColor.r) / 255.0f,
            static_cast<float>(m_clearColor.g) / 255.0f,
            static_cast<float>(m_clearColor.b) / 255.0f,
            static_cast<float>(m_clearColor.a) / 255.0f);
    }

    void Renderer::EndFrame()
    {
        m_renderAPI->DrawFPS(10, 10);
        m_graphicsContext->EndFrame();
    }
} // namespace Wayfinder
