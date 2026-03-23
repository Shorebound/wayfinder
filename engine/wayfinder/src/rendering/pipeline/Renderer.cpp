#include "Renderer.h"

#include "RenderContext.h"
#include "RenderPipeline.h"
#include "core/Result.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/graph/RenderFeature.h"
#include "rendering/graph/RenderFrame.h"
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
                WAYFINDER_WARNING(LogRenderer, "Renderer shutdown suppressed exception during destruction: {}", exception.what());
            }
            catch (...)
            {
                WAYFINDER_WARNING(LogRenderer, "Renderer shutdown suppressed unknown exception during destruction.");
            }
        }
    }

    Result<void> Renderer::Initialise(RenderDevice& device, const EngineConfig& config)
    {
        m_device = &device;
        m_screenWidth = static_cast<int>(config.Window.Width);
        m_screenHeight = static_cast<int>(config.Window.Height);
        // ── GPU infrastructure ───────────────────────────────
        m_context = std::make_unique<RenderContext>();
        if (auto result = m_context->Initialise(device, config); !result)
        {
            WAYFINDER_WARNING(LogRenderer, "Renderer: Failed to initialise RenderContext — {}", result.error().GetMessage());
            return std::unexpected(result.error());
        }

        // ── Render pipeline (registers shader programs) ──────
        m_renderPipeline->Initialise(*m_context);

        // Wire texture manager and program registry into resource cache
        m_renderResources->SetTextureManager(&m_context->GetTextures());
        m_renderResources->SetProgramRegistry(&m_context->GetPrograms());

        // ── Debug line pipeline (PosColour, uses debug_unlit shaders) ──
        {
            GPUPipelineDesc desc{};
            desc.vertexShaderName = "debug_unlit";
            desc.fragmentShaderName = "unlit"; // shares the same fragment shader
            desc.vertexResources = {.numUniformBuffers = 1};
            desc.fragmentResources = {.numUniformBuffers = 1};
            desc.vertexLayout = VertexLayouts::PosColour;
            desc.primitiveType = PrimitiveType::LineList;
            desc.cullMode = CullMode::None;
            desc.depthTestEnabled = false;
            desc.depthWriteEnabled = false;

            if (!m_debugLinePipeline.Create(device, m_context->GetShaders(), desc, &m_context->GetPipelines()))
            {
                WAYFINDER_WARNING(LogRenderer, "Renderer: Failed to create debug line pipeline");
            }
        }

        // Single built-in mesh for all scene primitives
        m_primitiveMesh = Mesh::CreatePrimitive(device);

        // UV-mapped mesh for textured rendering
        m_texturedPrimitiveMesh = Mesh::CreateTexturedPrimitive(device);

        // Attach any features that were added before Initialise().
        for (auto& feature : m_features)
        {
            auto ctx = MakeFeatureContext();
            feature->OnAttach(ctx);
        }

        WAYFINDER_INFO(LogRenderer, "Renderer initialised ({}x{}, backend: {})", m_screenWidth, m_screenHeight, device.GetDeviceInfo().BackendName);

        m_isInitialised = true;
        return {};
    }

    void Renderer::Shutdown()
    {
        // Remove features first (they may hold GPU resources)
        for (auto& feature : m_features)
        {
            auto ctx = MakeFeatureContext();
            feature->OnDetach(ctx);
        }
        m_features.clear();

        m_primitiveMesh.Destroy();
        m_texturedPrimitiveMesh.Destroy();
        m_debugLinePipeline.Destroy();

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

    RenderFeatureContext Renderer::MakeFeatureContext()
    {
        return RenderFeatureContext{
            .Device = m_context->GetDevice(),
            .ProgramRegistry = m_context->GetPrograms(),
            .ShaderManager = m_context->GetShaders(),
            .PipelineCache = m_context->GetPipelines(),
            .NearestSampler = m_context->GetNearestSampler(),
        };
    }

    void Renderer::AddFeature(std::unique_ptr<RenderFeature> feature)
    {
        if (m_device)
        {
            auto ctx = MakeFeatureContext();
            feature->OnAttach(ctx);
        }
        m_features.push_back(std::move(feature));
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

        m_context->GetTransientBuffers().BeginFrame();

        // Resolve material bindings from asset data
        RenderFrame preparedFrame = frame;
        if (m_renderResources)
        {
            m_renderResources->PrepareFrame(preparedFrame);
        }

        // Validate and sort passes via RenderPipeline
        if (!m_renderPipeline->Prepare(preparedFrame))
        {
            m_device->EndFrame();
            return;
        }

        // ── Query swapchain dimensions for transient targets ──
        const Extent2D swapchainDimensions = m_device->GetSwapchainDimensions();
        const uint32_t swapW = swapchainDimensions.width;
        const uint32_t swapH = swapchainDimensions.height;
        if (swapW == 0 || swapH == 0)
        {
            m_device->EndFrame();
            return;
        }

        // ── Build and execute render graph ───────────────────
        RenderGraph graph;

        const std::unordered_map<uint32_t, Mesh*> meshesByStride =
        {
            {VertexLayouts::PosNormalColour.stride, &m_primitiveMesh},
            {VertexLayouts::PosNormalUV.stride, &m_texturedPrimitiveMesh},
        };

        const RenderPipelineFrameParams params{
            .Frame = preparedFrame,
            .SwapchainWidth = swapW,
            .SwapchainHeight = swapH,
            .MeshesByStride = meshesByStride,
            .DebugLinePipeline = m_debugLinePipeline,
            .Features = m_features,
        };
        m_renderPipeline->BuildGraph(graph, params);

        if (graph.Compile())
        {
            graph.Execute(*m_device, m_context->GetTransientPool());
        }

        m_device->EndFrame();
    }

} // namespace Wayfinder
