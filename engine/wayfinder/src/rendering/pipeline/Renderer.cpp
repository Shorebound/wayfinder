#include "Renderer.h"

#include "RenderServices.h"
#include "core/Result.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/graph/RenderFrame.h"
#include "rendering/graph/RenderFrameUtils.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/pipeline/DefaultFeatures.h"
#include "rendering/pipeline/PrepareFrame.h"
#include "rendering/resources/RenderResourceCache.h"

#include "app/EngineConfig.h"
#include "core/Log.h"
#include "core/Types.h"
#include "rendering/RenderTypes.h"

namespace Wayfinder
{
    Renderer::Renderer() : m_screenWidth(800), m_screenHeight(450), m_isInitialised(false)
    {
        m_renderPipeline = std::make_unique<RenderOrchestrator>();
        m_renderResources = std::make_unique<RenderResourceCache>();
    }

    Renderer::~Renderer() noexcept
    {
        if (m_isInitialised)
        {
            try
            {
                Shutdown();
            }
            catch (const std::exception& exception)
            {
                WAYFINDER_WARN(LogRenderer, "Renderer shutdown suppressed exception during destruction: {}", exception.what());
            }
            catch (...)
            {
                WAYFINDER_WARN(LogRenderer, "Renderer shutdown suppressed unknown exception during destruction.");
            }
        }
    }

    Result<void> Renderer::Initialise(RenderDevice& device, const EngineConfig& config, BlendableEffectRegistry* registry)
    {
        if (m_isInitialised)
        {
            Shutdown();
        }

        m_device = &device;
        m_screenWidth = static_cast<int>(config.Window.Width);
        m_screenHeight = static_cast<int>(config.Window.Height);
        m_services = std::make_unique<RenderServices>();
        if (auto result = m_services->Initialise(device, config, registry); !result)
        {
            WAYFINDER_WARN(LogRenderer, "Renderer: Failed to initialise RenderServices -- {}", result.error().GetMessage());
            return std::unexpected(result.error());
        }

        Rendering::RegisterDefaultFeatures(*this);
        m_renderPipeline->Initialise(*m_services);

        m_renderResources->SetTextureManager(&m_services->GetTextures());
        m_renderResources->SetMeshManager(&m_services->GetMeshes());
        m_renderResources->SetProgramRegistry(&m_services->GetPrograms());

        m_isInitialised = true;

        WAYFINDER_INFO(LogRenderer, "Renderer initialised ({}x{}, backend: {})", m_screenWidth, m_screenHeight, device.GetDeviceInfo().BackendName);

        return {};
    }

    void Renderer::Shutdown()
    {
        m_renderPipeline->Shutdown();

        if (m_services)
        {
            m_services->Shutdown();
            m_services.reset();
        }

        m_renderPipeline = std::make_unique<RenderOrchestrator>();
        m_renderResources = std::make_unique<RenderResourceCache>();
        if (m_assetService)
        {
            m_renderResources->SetAssetService(m_assetService);
        }
        m_device = nullptr;
        m_isInitialised = false;
    }

    void Renderer::SetAssetService(const std::shared_ptr<AssetService>& assetService)
    {
        m_assetService = assetService;
        if (m_renderResources)
        {
            m_renderResources->SetAssetService(assetService);
        }
    }

    void Renderer::SealBlendableEffects()
    {
        if (m_services)
        {
            m_services->SealBlendableEffects();
        }
    }

    void Renderer::AddFeature(const RenderPhase phase, const int32_t order, std::unique_ptr<RenderFeature> feature)
    {
        if (!feature)
        {
            return;
        }
        if (m_renderPipeline)
        {
            m_renderPipeline->RegisterFeature(phase, order, std::move(feature));
        }
    }

    void Renderer::AddFeature(const RenderPhase phase, std::unique_ptr<RenderFeature> feature)
    {
        AddFeature(phase, 0, std::move(feature));
    }

    void Renderer::Render(const RenderFrame& frame)
    {
        if (!m_isInitialised || !m_device)
        {
            return;
        }

        // Auto-seal the blendable effect registry on first render if not yet sealed.
        if (m_services)
        {
            m_services->SealBlendableEffects();
        }

        if (!m_device->BeginFrame())
        {
            return;
        }

        {
            std::string frameDebugName = "Frame";
            if (!frame.SceneName.empty())
            {
                frameDebugName += ": ";
                frameDebugName += frame.SceneName;
            }

            const GPUDebugScope frameDebugScope(*m_device, frameDebugName);

            m_services->GetTransientBuffers().BeginFrame();

            RenderFrame preparedFrame = frame;
            if (m_renderResources)
            {
                m_renderResources->PrepareFrame(preparedFrame);
            }

            const Extent2D swapchainDimensions = m_device->GetSwapchainDimensions();
            const uint32_t swapW = swapchainDimensions.Width;
            const uint32_t swapH = swapchainDimensions.Height;

            if (swapW != 0 && swapH != 0 && Rendering::PrepareFrame(preparedFrame, swapW, swapH))
            {
                const auto primaryView = Rendering::ResolvePreparedPrimaryView(preparedFrame);

                RenderGraph graph;

                const FrameRenderParams params{
                    .Frame = preparedFrame,
                    .SwapchainWidth = swapW,
                    .SwapchainHeight = swapH,
                    .BuiltInMeshes = m_services->GetBuiltInMeshes(),
                    .ResourceCache = m_renderResources.get(),
                    .PrimaryView = primaryView,
                };
                m_renderPipeline->BuildGraph(graph, params);

                if (graph.Compile())
                {
                    m_device->FlushUploads();
                    graph.Execute(*m_device, m_services->GetTransientPool());
                }
            }
        }

        m_device->EndFrame();
    }

} // namespace Wayfinder
