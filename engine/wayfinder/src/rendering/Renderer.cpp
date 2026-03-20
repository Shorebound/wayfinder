#include "Renderer.h"

#include "RenderContext.h"
#include "RenderDevice.h"
#include "RenderFeature.h"
#include "RenderFrame.h"
#include "RenderGraph.h"
#include "RenderPipeline.h"
#include "RenderResources.h"

#include "../core/EngineConfig.h"
#include "../core/Log.h"

namespace Wayfinder
{
    Renderer::Renderer()
        : m_screenWidth(800), m_screenHeight(450), m_isInitialised(false)
    {
        m_renderPipeline = std::make_unique<RenderPipeline>();
        m_renderResources = std::make_unique<RenderResourceCache>();
    }

    Renderer::~Renderer()
    {
        if (m_isInitialised)
        {
            Shutdown();
        }
    }

    bool Renderer::Initialise(RenderDevice& device, const EngineConfig& config)
    {
        m_device = &device;
        m_screenWidth = static_cast<int>(config.Window.Width);
        m_screenHeight = static_cast<int>(config.Window.Height);
        m_isInitialised = true;

        // ── GPU infrastructure ───────────────────────────────
        m_context = std::make_unique<RenderContext>();
        if (!m_context->Initialise(device, config))
        {
            WAYFINDER_WARNING(LogRenderer, "Renderer: Failed to initialise RenderContext");
            return false;
        }

        // ── Render pipeline (registers shader programs) ──────
        m_renderPipeline->Initialise(*m_context);

        // ── Debug line pipeline (PosColor, uses debug_unlit shaders) ──
        {
            GPUPipelineDesc desc{};
            desc.vertexShaderName = "debug_unlit";
            desc.fragmentShaderName = "unlit"; // shares the same fragment shader
            desc.vertexResources = {.numUniformBuffers = 1};
            desc.fragmentResources = {.numUniformBuffers = 1};
            desc.vertexLayout = VertexLayouts::PosColor;
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

        // Attach any features that were added before Initialise().
        for (auto& feature : m_features)
        {
            auto ctx = MakeFeatureContext();
            feature->OnAttach(ctx);
        }

        WAYFINDER_INFO(LogRenderer, "Renderer initialised ({}x{}, backend: {})",
            m_screenWidth, m_screenHeight, device.GetDeviceInfo().BackendName);

        return true;
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
        if (m_device) { auto ctx = MakeFeatureContext(); feature->OnAttach(ctx); }
        m_features.push_back(std::move(feature));
    }



    void Renderer::Render(const RenderFrame& frame)
    {
        if (!m_isInitialised || !m_device) return;
        if (!m_device->BeginFrame()) return;

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
        uint32_t swapW = 0, swapH = 0;
        m_device->GetSwapchainDimensions(swapW, swapH);
        if (swapW == 0 || swapH == 0)
        {
            m_device->EndFrame();
            return;
        }

        // ── Build and execute render graph ───────────────────
        RenderGraph graph;
        RenderPipelineFrameParams params{
            .Frame = preparedFrame,
            .SwapchainWidth = swapW,
            .SwapchainHeight = swapH,
            .PrimitiveMesh = m_primitiveMesh,
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
