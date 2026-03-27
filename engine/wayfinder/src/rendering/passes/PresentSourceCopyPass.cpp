#include "PresentSourceCopyPass.h"

#include "rendering/backend/GPUPipeline.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/backend/VertexFormats.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/graph/RenderPassCapabilities.h"
#include "rendering/materials/PostProcessVolume.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/pipeline/CompositionUBOUtils.h"
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
        // Same SPIR-V as `composition` — identity grading so this stays a handoff blit without a separate deploy target.
        // @todo Pipeline assumes SwapchainFormat colour targets (GPUPipelineDesc has no colourTargetFormats field).
        //       This pass renders to RGBA8_UNORM transient; if activated, extend ShaderProgramDesc → GPUPipelineDesc
        //       with colourTargetFormats so the pipeline matches the render target format.
        desc.FragmentShaderName = "composition";
        desc.VertexResources = {};
        desc.FragmentResources = {.numUniformBuffers = 1, .numSamplers = 1};
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
            // Not FULLSCREEN_COMPOSITE: that capability is validated as "writes swapchain"; this pass writes a transient RT.
            builder.DeclarePassCapabilities(RenderPassCapabilities::RASTER);
            const auto sceneColour = graph.FindHandleChecked(GraphTextureId::SceneColour);
            builder.ReadTexture(sceneColour);

            RenderGraphTextureDesc presentDesc{};
            presentDesc.Width = w;
            presentDesc.Height = h;
            presentDesc.Format = TextureFormat::RGBA8_UNORM;
            presentDesc.DebugName = GraphTextureName(GraphTextureId::PresentSource);
            const auto present = builder.CreateTransient(presentDesc);
            // Clear so a failed draw does not leave undefined VRAM (Composition samples this when the pass is registered).
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

                const CompositionUBO ubo = MakeCompositionUBO(ColourGradingParams{});

                copyProgram->Pipeline->Bind();
                device.BindFragmentSampler(0, src, nearestSampler);
                device.PushFragmentUniform(0, &ubo, sizeof(CompositionUBO));
                device.DrawPrimitives(3);
            };
        });
    }
} // namespace Wayfinder
