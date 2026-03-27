#include "CompositionPass.h"

#include "rendering/backend/GPUPipeline.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/backend/VertexFormats.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/graph/RenderPassCapabilities.h"
#include "rendering/materials/RenderingEffects.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/pipeline/CompositionUBOUtils.h"
#include "rendering/pipeline/RenderContext.h"

#include "core/Log.h"
#include "core/Types.h"

namespace Wayfinder
{
    void CompositionPass::OnAttach(const RenderPassContext& context)
    {
        m_context = &context.Context;
        auto& registry = context.Context.GetPrograms();

        ShaderProgramDesc desc;
        desc.Name = "composition";
        desc.VertexShaderName = "fullscreen";
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

        if (!registry.Register(desc))
        {
            WAYFINDER_ERROR(LogRenderer, "CompositionPass: failed to register 'composition' shader program — check assets/shaders and working directory; "
                                         "composition draws will be skipped");
        }
    }

    void CompositionPass::OnDetach(const RenderPassContext& /*context*/)
    {
        m_context = nullptr;
    }

    void CompositionPass::AddPasses(RenderGraph& graph, const RenderPipelineFrameParams& params)
    {
        if (!m_context)
        {
            WAYFINDER_WARN(LogRenderer, "CompositionPass: no context — skipped");
            return;
        }

        const RenderGraphHandle presentHandle = graph.FindHandle(GraphTextureId::PresentSource);
        const bool usePresentSource = presentHandle.IsValid();
        const RenderGraphHandle colourHandle = usePresentSource ? presentHandle : graph.FindHandleChecked(GraphTextureId::SceneColour);
        if (!usePresentSource)
        {
            WAYFINDER_VERBOSE(LogRenderer, "CompositionPass: PresentSource not in graph — sampling SceneColour (register PresentSourceCopyPass or a pass "
                                           "that writes PresentSource when you need a handoff texture).");
        }

        graph.AddPass("Composition", [&](RenderGraphBuilder& builder)
        {
            builder.DeclarePassCapabilities(RenderPassCapabilities::RASTER | RenderPassCapabilities::FULLSCREEN_COMPOSITE);
            builder.ReadTexture(colourHandle);
            // Renderer clears the swapchain before the graph; load existing pixels then overwrite with the fullscreen triangle.
            // Avoids a second full-frame clear and keeps the backbuffer defined if this pass's draw is skipped.
            builder.SetSwapchainOutput(LoadOp::Load);

            return [this, colourHandle, &params](RenderDevice& device, const RenderGraphResources& resources)
            {
                auto sceneColourTex = resources.GetTexture(colourHandle);

                const ShaderProgram* compProgram = m_context->GetPrograms().Find("composition");
                const auto nearestSampler = m_context->GetNearestSampler();
                if (!compProgram || !compProgram->Pipeline || !sceneColourTex || !nearestSampler)
                {
                    WAYFINDER_ERROR(LogRenderer, "Composition: missing program, pipeline, colour input, or sampler — skipped");
                    return;
                }

                ColourGradingParams grading{};
                VignetteParams vignette{};
                ChromaticAberrationParams chromaticAberration{};
                if (params.PrimaryView.Valid && !params.Frame.Views.empty())
                {
                    const VolumeEffectStack& stack = params.Frame.Views.front().PostProcess;
                    const EngineEffectIds& ids = m_context->GetEngineEffectIds();
                    grading = ResolveColourGradingForView(stack, ids.ColourGrading);
                    vignette = ResolveVignetteForView(stack, ids.Vignette);
                    chromaticAberration = ResolveChromaticAberrationForView(stack, ids.ChromaticAberration);
                }
                const CompositionUBO ubo = MakeCompositionUBO(grading, vignette, chromaticAberration);

                compProgram->Pipeline->Bind();
                device.BindFragmentSampler(0, sceneColourTex, nearestSampler);
                device.PushFragmentUniform(0, &ubo, sizeof(CompositionUBO));
                device.DrawPrimitives(3);
            };
        });
    }
} // namespace Wayfinder
