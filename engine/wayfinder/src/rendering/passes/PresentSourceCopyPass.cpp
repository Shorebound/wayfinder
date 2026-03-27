#include "PresentSourceCopyPass.h"

#include "rendering/backend/GPUPipeline.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/backend/VertexFormats.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/graph/RenderPassCapabilities.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/pipeline/RenderContext.h"

#include "core/Log.h"
#include "core/Types.h"

namespace Wayfinder
{
    void PresentSourceCopyPass::OnAttach(const RenderPassContext& context)
    {
        m_context = &context.Context;
        auto& registry = context.Context.GetPrograms();

        ShaderProgramDesc desc;
        desc.Name = "present_source_copy";
        desc.VertexShaderName = "fullscreen";
        desc.FragmentShaderName = "fullscreen_copy";
        desc.VertexResources = {};
        desc.FragmentResources = {.numUniformBuffers = 0, .numSamplers = 1};
        desc.VertexLayout = VertexLayouts::Empty;
        desc.Cull = CullMode::None;
        desc.DepthTest = false;
        desc.DepthWrite = false;
        desc.MaterialUBOSize = 0;
        desc.VertexUBOSize = 0;
        desc.NeedsSceneGlobals = false;

        registry.Register(desc);
    }

    void PresentSourceCopyPass::OnDetach(const RenderPassContext& /*context*/)
    {
        m_context = nullptr;
    }

    void PresentSourceCopyPass::AddPasses(RenderGraph& graph, const RenderPipelineFrameParams& params)
    {
        if (!m_context)
        {
            WAYFINDER_WARN(LogRenderer, "PresentSourceCopyPass: no context — skipped");
            return;
        }

        const uint32_t w = params.SwapchainWidth;
        const uint32_t h = params.SwapchainHeight;
        if (w == 0 || h == 0)
        {
            return;
        }

        graph.AddPass("PresentSourceCopy", [&](RenderGraphBuilder& builder)
        {
            builder.DeclarePassCapabilities(RenderPassCapabilities::RASTER);
            const auto sceneColour = graph.FindHandleChecked(GraphTextureId::SceneColour);
            builder.ReadTexture(sceneColour);

            RenderGraphTextureDesc presentDesc{};
            presentDesc.Width = w;
            presentDesc.Height = h;
            presentDesc.Format = TextureFormat::RGBA8_UNORM;
            presentDesc.DebugName = GraphTextureName(GraphTextureId::PresentSource);
            const auto present = builder.CreateTransient(presentDesc);
            builder.WriteColour(present, LoadOp::Clear, ClearValue::FromColour(Colour::Black()));

            return [this, sceneColour](RenderDevice& device, const RenderGraphResources& resources)
            {
                const auto src = resources.GetTexture(sceneColour);
                const ShaderProgram* copyProgram = m_context->GetPrograms().Find("present_source_copy");
                const auto nearestSampler = m_context->GetNearestSampler();
                if (!copyProgram || !copyProgram->Pipeline || !src || !nearestSampler)
                {
                    WAYFINDER_ERROR(LogRenderer, "PresentSourceCopy: missing program, pipeline, texture, or sampler — skipped");
                    return;
                }

                copyProgram->Pipeline->Bind();
                device.BindFragmentSampler(0, src, nearestSampler);
                device.DrawPrimitives(3);
            };
        });
    }
} // namespace Wayfinder
