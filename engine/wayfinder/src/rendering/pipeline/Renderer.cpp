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
#include "core/Types.h"
#include "rendering/RenderTypes.h"
#include "rendering/backend/VertexFormats.h"

#include <unordered_map>

namespace Wayfinder
{
    namespace
    {
        /**
         * Clears the swapchain to a defined colour in the current command buffer.
         *
         * Call this once per frame after BeginFrame (when dimensions are valid) before BuildGraph/Execute. Swapchain images
         * start undefined; without an initial clear, any later pass that fails to begin (or skips its draw) can leave
         * undefined memory visible — static noise and per-frame "shimmer" as different garbage is presented each frame.
         */
        void ClearSwapchainToColour(RenderDevice& device, const ClearValue& clear)
        {
            RenderPassDescriptor rp{};
            rp.debugName = "SwapchainInitialClear";
            rp.targetSwapchain = true;
            rp.numColourTargets = 1;
            rp.colourAttachments[0].loadOp = LoadOp::Clear;
            rp.colourAttachments[0].clearValue = clear;
            rp.colourAttachments[0].storeOp = StoreOp::Store;
            rp.depthAttachment.enabled = false;

            if (!device.BeginRenderPass(rp))
            {
                WAYFINDER_ERROR(LogRenderer, "ClearSwapchainToColour: BeginRenderPass failed — swapchain may show garbage");
                return;
            }
            device.EndRenderPass();
        }
    } // namespace

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

        m_isInitialised = true;

        {
            auto pending = std::move(m_pendingPasses);
            m_pendingPasses.clear();
            for (auto& p : pending)
            {
                m_renderPipeline->RegisterPass(p.Phase, p.Order, std::move(p.Pass));
            }
        }

        WAYFINDER_INFO(LogRenderer, "Renderer initialised ({}x{}, backend: {})", m_screenWidth, m_screenHeight, device.GetDeviceInfo().BackendName);

        return {};
    }

    void Renderer::Shutdown()
    {
        m_pendingPasses.clear();

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

    void Renderer::SealBlendableEffects()
    {
        if (m_context)
        {
            m_context->SealBlendableEffects();
        }
    }

    RenderPassContext Renderer::MakePassContext()
    {
        return RenderPassContext{.Context = *m_context};
    }

    void Renderer::AddPass(const RenderPhase phase, const int32_t order, std::unique_ptr<RenderPass> pass)
    {
        if (!pass)
        {
            return;
        }
        if (m_isInitialised && m_context && m_renderPipeline)
        {
            m_renderPipeline->RegisterPass(phase, order, std::move(pass));
        }
        else
        {
            m_pendingPasses.push_back(PendingPassRegistration{.Phase = phase, .Order = order, .Pass = std::move(pass)});
        }
    }

    void Renderer::AddPass(const RenderPhase phase, std::unique_ptr<RenderPass> pass)
    {
        AddPass(phase, 0, std::move(pass));
    }

    void Renderer::Render(const RenderFrame& frame)
    {
        if (!m_isInitialised || !m_device)
        {
            return;
        }

        // Auto-seal the blendable effect registry on first render if not yet sealed.
        if (m_context)
        {
            m_context->SealBlendableEffects();
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

            // Derive clear colour directly — ResolvePreparedPrimaryView requires Prepare() which hasn't run yet.
            const ClearValue initialClear = (!preparedFrame.Views.empty()) ? ClearValue::FromColour(preparedFrame.Views.front().ClearColour) : ClearValue::FromColour(Colour::Black());

            // Define swapchain contents before any graph pass. Do not gate on GetSwapchainDimensions(): some backends can
            // report 0x0 briefly while a swapchain texture is still acquired; skipping the clear then leaves LoadOp::Load
            // in Composition sampling garbage from SceneColour against an undefined backbuffer.
            ClearSwapchainToColour(*m_device, initialClear);

            if (swapW != 0 && swapH != 0 && m_renderPipeline->Prepare(preparedFrame, swapW, swapH))
            {
                const auto primaryView = Rendering::ResolvePreparedPrimaryView(preparedFrame);

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
                    .PrimaryView = primaryView,
                };
                m_renderPipeline->BuildGraph(graph, params);

                if (graph.Compile())
                {
                    graph.Execute(*m_device, m_context->GetTransientPool());
                }
            }
        }

        m_device->EndFrame();
    }

} // namespace Wayfinder
