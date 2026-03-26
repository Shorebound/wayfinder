#include "Renderer.h"

#include "RenderContext.h"
#include "core/Result.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/graph/RenderFrame.h"
#include "rendering/graph/RenderFrameUtils.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/resources/RenderResources.h"

#include "app/EngineConfig.h"
#include "core/Log.h"
#include "rendering/backend/VertexFormats.h"

namespace Wayfinder
{
    Renderer::Renderer() : m_screenWidth(800), m_screenHeight(450), m_isInitialised(false)
    {
        m_renderPipeline = std::make_unique<RenderPipeline>();
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

    Result<void> Renderer::Initialise(RenderDevice& device, const EngineConfig& config)
    {
        m_device = &device;
        m_screenWidth = static_cast<int>(config.Window.Width);
        m_screenHeight = static_cast<int>(config.Window.Height);
        m_context = std::make_unique<RenderContext>();
        if (auto result = m_context->Initialise(device, config); !result)
        {
            WAYFINDER_WARN(LogRenderer, "Renderer: Failed to initialise RenderContext — {}", result.error().GetMessage());
            return std::unexpected(result.error());
        }

        m_renderPipeline->Initialise(*m_context);

        m_renderResources->SetTextureManager(&m_context->GetTextures());
        m_renderResources->SetMeshManager(&m_context->GetMeshes());
        m_renderResources->SetProgramRegistry(&m_context->GetPrograms());

        m_primitiveMesh = Mesh::CreatePrimitive(device);
        m_texturedPrimitiveMesh = Mesh::CreateTexturedPrimitive(device);

        for (auto& pass : m_passes)
        {
            auto ctx = MakePassContext();
            pass->OnAttach(ctx);
        }

        WAYFINDER_INFO(LogRenderer, "Renderer initialised ({}x{}, backend: {})", m_screenWidth, m_screenHeight, device.GetDeviceInfo().BackendName);

        m_isInitialised = true;
        return {};
    }

    void Renderer::Shutdown()
    {
        for (auto& pass : m_passes)
        {
            if (m_isInitialised && m_context)
            {
                auto ctx = MakePassContext();
                pass->OnDetach(ctx);
            }
        }
        m_passes.clear();

        m_primitiveMesh.Destroy();
        m_texturedPrimitiveMesh.Destroy();

        m_renderPipeline->Shutdown();

        if (m_context)
        {
            m_context->Shutdown();
            m_context.reset();
        }

        m_renderPipeline = std::make_unique<RenderPipeline>();
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

    RenderPassContext Renderer::MakePassContext()
    {
        return RenderPassContext{.Context = *m_context};
    }

    void Renderer::AddPass(std::unique_ptr<RenderPass> pass)
    {
        if (!pass)
        {
            return;
        }
        if (m_isInitialised && m_context)
        {
            auto ctx = MakePassContext();
            pass->OnAttach(ctx);
        }
        m_passes.push_back(std::move(pass));
    }

    void Renderer::RegisterEnginePass(EngineRenderPhase phase, int32_t orderWithinPhase, std::unique_ptr<RenderPass> pass)
    {
        if (!m_renderPipeline)
        {
            return;
        }
        m_renderPipeline->RegisterEnginePass(phase, orderWithinPhase, std::move(pass));
    }

    void Renderer::Render(const RenderFrame& frame)
    {
        if (!m_isInitialised || !m_device)
        {
            return;
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

            m_context->GetTransientBuffers().BeginFrame();

            RenderFrame preparedFrame = frame;
            if (m_renderResources)
            {
                m_renderResources->PrepareFrame(preparedFrame);
            }

            const Extent2D swapchainDimensions = m_device->GetSwapchainDimensions();
            const uint32_t swapW = swapchainDimensions.width;
            const uint32_t swapH = swapchainDimensions.height;
            if (swapW != 0 && swapH != 0 && m_renderPipeline->Prepare(preparedFrame, swapW, swapH))
            {
                RenderGraph graph;

                const std::unordered_map<uint32_t, Mesh*> meshesByStride =
                {
                    {VertexLayouts::PosNormalColour.stride, &m_primitiveMesh},
                    {VertexLayouts::PosNormalUVTangent.stride, &m_texturedPrimitiveMesh},
                };

                const RenderPipelineFrameParams params{
                    .Frame = preparedFrame,
                    .SwapchainWidth = swapW,
                    .SwapchainHeight = swapH,
                    .MeshesByStride = meshesByStride,
                    .ResourceCache = m_renderResources.get(),
                    .PrimaryView = Rendering::ResolvePreparedPrimaryView(preparedFrame),
                };
                m_renderPipeline->BuildGraph(graph, params, m_passes);

                if (graph.Compile())
                {
                    graph.Execute(*m_device, m_context->GetTransientPool());
                }
            }
        }

        m_device->EndFrame();
    }

} // namespace Wayfinder
