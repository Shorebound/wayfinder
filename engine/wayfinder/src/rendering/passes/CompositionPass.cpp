#include "CompositionPass.h"

#include "rendering/backend/RenderDevice.h"
#include "rendering/backend/VertexFormats.h"
#include "rendering/graph/RenderCapabilities.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/materials/RenderingEffects.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/pipeline/BuiltInUBOs.h"
#include "rendering/pipeline/RenderServices.h"

#include "core/Log.h"
#include "core/Types.h"

namespace Wayfinder
{
    void CompositionPass::OnAttach(const RenderFeatureContext& context)
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

    void CompositionPass::OnDetach(const RenderFeatureContext& /*context*/)
    {
        m_context = nullptr;
    }

    void CompositionPass::AddPasses(RenderGraph& graph, const FrameRenderParams& params)
    {
        if (!m_context)
        {
            WAYFINDER_WARN(LogRenderer, "CompositionPass: no context — skipped");
            return;
        }

        const RenderGraphHandle colourHandle = ResolvePostProcessInput(graph);

        graph.AddPass("Composition", [&](RenderGraphBuilder& builder)
        {
            builder.DeclarePassCapabilities(RenderCapabilities::RASTER | RenderCapabilities::FULLSCREEN_COMPOSITE);
            builder.ReadTexture(colourHandle);

            // Derive the clear colour from the primary view (falls back to black).
            const Colour clearColour = (params.PrimaryView.Valid && !params.Frame.Views.empty()) ? params.Frame.Views.front().ClearColour : Colour::Black();
            builder.SetSwapchainOutput(LoadOp::Clear, ClearValue::FromColour(clearColour));

            return [this, colourHandle, &params](RenderDevice& device, const RenderGraphResources& resources)
            {
                auto sceneColourTex = resources.GetTexture(colourHandle);

                const ShaderProgram* compProgram = m_context->GetPrograms().Find("composition");
                const auto nearestSampler = m_context->GetNearestSampler();
                if (!compProgram || !compProgram->Pipeline.IsValid() || !sceneColourTex || !nearestSampler)
                {
                    WAYFINDER_ERROR(LogRenderer, "Composition: missing program, pipeline, colour input, or sampler — skipped");
                    return;
                }

                ColourGradingParams grading{};
                VignetteParams vignette{};
                ChromaticAberrationParams chromaticAberration{};
                if (params.PrimaryView.Valid && !params.Frame.Views.empty())
                {
                    const BlendableEffectStack& stack = params.Frame.Views.front().PostProcess;
                    const EngineEffectIds& ids = m_context->GetEngineEffectIds();
                    grading = ResolveColourGradingForView(stack, ids.ColourGrading);
                    vignette = ResolveVignetteForView(stack, ids.Vignette);
                    chromaticAberration = ResolveChromaticAberrationForView(stack, ids.ChromaticAberration);
                }
                const CompositionUBO ubo = MakeCompositionUBO(grading, vignette, chromaticAberration);

                device.BindPipeline(compProgram->Pipeline);
                device.BindFragmentSampler(0, sceneColourTex, nearestSampler);
                device.PushFragmentUniform(0, &ubo, sizeof(CompositionUBO));
                device.DrawPrimitives(3);
            };
        });
    }
} // namespace Wayfinder
