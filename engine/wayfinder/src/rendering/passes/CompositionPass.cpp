#include "CompositionPass.h"

#include "rendering/backend/VertexFormats.h"
#include "rendering/graph/RenderCapabilities.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/pipeline/RenderServices.h"

#include "core/Log.h"
#include "core/Types.h"

namespace Wayfinder
{
    std::span<const ShaderProgramDesc> CompositionPass::GetShaderPrograms() const
    {
        static const auto PROGRAMS = []
        {
            ShaderProgramDesc desc;
            desc.Name = "composition_blit";
            desc.VertexShaderName = "fullscreen_copy";
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
            return std::vector{std::move(desc)};
        }();

        return PROGRAMS;
    }

    void CompositionPass::OnAttach(const RenderFeatureContext& context)
    {
        m_context = &context.Context;
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

            const Colour clearColour = (params.PrimaryView.Valid && params.PrimaryView.ViewIndex < params.Frame.Views.size()) ? params.Frame.Views[params.PrimaryView.ViewIndex].ClearColour : Colour::Black();
            builder.SetSwapchainOutput(LoadOp::Clear, ClearValue::FromColour(clearColour));

            return [this, colourHandle](RenderDevice& device, const RenderGraphResources& resources)
            {
                auto sceneColourTex = resources.GetTexture(colourHandle);

                const ShaderProgram* compProgram = m_context->GetPrograms().Find("composition_blit");
                const auto nearestSampler = m_context->GetNearestSampler();
                if (!compProgram || !compProgram->Pipeline.IsValid() || !sceneColourTex || !nearestSampler)
                {
                    WAYFINDER_ERROR(LogRenderer, "Composition: missing program, pipeline, colour input, or sampler — skipped");
                    return;
                }

                device.BindPipeline(compProgram->Pipeline);
                device.BindFragmentSampler(0, sceneColourTex, nearestSampler);
                device.DrawPrimitives(3);
            };
        });
    }
} // namespace Wayfinder
