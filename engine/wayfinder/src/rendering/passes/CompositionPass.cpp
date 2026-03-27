#include "CompositionPass.h"

#include "rendering/backend/GPUPipeline.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/graph/RenderPassCapabilities.h"
#include "rendering/materials/PostProcessVolume.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/pipeline/RenderContext.h"
#include "rendering/pipeline/ShaderUniforms.h"

#include "core/Log.h"

#include "rendering/backend/VertexFormats.h"

namespace Wayfinder
{
    namespace
    {
        CompositionUBO MakeCompositionUBO(const ColourGradingParams& p)
        {
            CompositionUBO u{};
            u.ExposureContrastSaturationPad = Float4(p.ExposureStops, p.Contrast, p.Saturation, 0.0f);
            u.Lift = Float4(p.Lift.x, p.Lift.y, p.Lift.z, 0.0f);
            u.Gamma = Float4(p.Gamma.x, p.Gamma.y, p.Gamma.z, 0.0f);
            u.Gain = Float4(p.Gain.x, p.Gain.y, p.Gain.z, 0.0f);
            u.VignetteAberrationPad = Float4(p.VignetteStrength, p.ChromaticAberrationIntensity, 0.0f, 0.0f);
            return u;
        }

    } // namespace

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

        registry.Register(desc);
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

        graph.AddPass("Composition", [&](RenderGraphBuilder& builder)
        {
            builder.DeclarePassCapabilities(RenderPassCapabilities::RASTER | RenderPassCapabilities::FULLSCREEN_COMPOSITE);
            auto present = graph.FindHandleChecked(GraphTextureId::PresentSource);
            builder.ReadTexture(present);
            builder.SetSwapchainOutput(LoadOp::DontCare);

            return [this, present, &params](RenderDevice& device, const RenderGraphResources& resources)
            {
                auto sceneColourTex = resources.GetTexture(present);

                const ShaderProgram* compProgram = m_context->GetPrograms().Find("composition");
                const auto nearestSampler = m_context->GetNearestSampler();
                if (!compProgram || !compProgram->Pipeline || !sceneColourTex || !nearestSampler)
                {
#if defined(NDEBUG)
                    WAYFINDER_WARN(LogRenderer, "Composition: missing program, pipeline, present source, or sampler — skipped");
#else
                    WAYFINDER_ERROR(LogRenderer, "Composition: missing program, pipeline, present source, or sampler — skipped");
#endif
                    return;
                }

                auto grading = ColourGradingParams{};
                if (params.PrimaryView.Valid && !params.Frame.Views.empty())
                {
                    grading = ResolveColourGradingForView(params.Frame.Views.front().PostProcess);
                }
                const CompositionUBO ubo = MakeCompositionUBO(grading);

                compProgram->Pipeline->Bind();
                device.BindFragmentSampler(0, sceneColourTex, nearestSampler);
                device.PushFragmentUniform(0, &ubo, sizeof(CompositionUBO));
                device.DrawPrimitives(3);
            };
        });
    }
} // namespace Wayfinder
