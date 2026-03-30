#include "ChromaticAberrationFeature.h"

#include "rendering/backend/VertexFormats.h"
#include "rendering/graph/RenderCapabilities.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/materials/RenderingEffects.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/pipeline/RenderServices.h"
#include "volumes/BlendableEffect.h"
#include "volumes/BlendableEffectUtils.h"

#include "core/Log.h"
#include "core/Types.h"

namespace Wayfinder
{
    namespace
    {
        /// Matches `chromatic_aberration.slang` fragment UBO (std140).
        struct alignas(16) ChromaticAberrationUBO
        {
            Float4 IntensityPad{};
        };
        static_assert(sizeof(ChromaticAberrationUBO) == 16);
    } // namespace

    RenderCapabilityMask ChromaticAberrationFeature::GetCapabilities() const
    {
        return RenderCapabilities::RASTER;
    }

    std::span<const ShaderProgramDesc> ChromaticAberrationFeature::GetShaderPrograms() const
    {
        static const auto programs = []
        {
            ShaderProgramDesc desc;
            desc.Name = "chromatic_aberration";
            desc.VertexShaderName = "chromatic_aberration";
            desc.FragmentShaderName = "chromatic_aberration";
            desc.VertexResources = {};
            desc.FragmentResources = {.numUniformBuffers = 1, .numSamplers = 1};
            desc.VertexLayout = VertexLayouts::Empty;
            desc.Cull = CullMode::None;
            desc.DepthTest = false;
            desc.DepthWrite = false;
            desc.MaterialUBOSize = sizeof(ChromaticAberrationUBO);
            desc.VertexUBOSize = 0;
            desc.NeedsSceneGlobals = false;
            return std::vector{std::move(desc)};
        }();

        return programs;
    }

    void ChromaticAberrationFeature::OnRegisterEffects(BlendableEffectRegistry& registry)
    {
        m_effectId = registry.Register<ChromaticAberrationParams>("chromatic_aberration");
        if (m_effectId == INVALID_BLENDABLE_EFFECT_ID)
        {
            WAYFINDER_ERROR(LogRenderer, "ChromaticAberrationFeature: failed to register chromatic_aberration blendable type");
        }
    }

    void ChromaticAberrationFeature::OnAttach(const RenderFeatureContext& context)
    {
        m_context = &context.Context;
    }

    void ChromaticAberrationFeature::OnDetach(const RenderFeatureContext& /*context*/)
    {
        m_context = nullptr;
        m_effectId = INVALID_BLENDABLE_EFFECT_ID;
    }

    void ChromaticAberrationFeature::AddPasses(RenderGraph& graph, const FrameRenderParams& params)
    {
        if (!m_context || m_effectId == INVALID_BLENDABLE_EFFECT_ID)
        {
            return;
        }

        const BlendableEffectStack* stack = nullptr;
        if (params.PrimaryView.Valid && params.PrimaryView.ViewIndex < params.Frame.Views.size())
        {
            stack = &params.Frame.Views[params.PrimaryView.ViewIndex].PostProcess;
        }
        const BlendableEffectStack emptyStack{};
        const BlendableEffectStack& s = stack ? *stack : emptyStack;
        const auto ca = ResolveEffect<ChromaticAberrationParams>(s, m_effectId);
        if (ca.Intensity.Value <= 0.0f)
        {
            return;
        }

        const RenderGraphHandle inputHandle = ResolvePostProcessInput(graph);
        const uint32_t w = params.SwapchainWidth;
        const uint32_t h = params.SwapchainHeight;

        graph.AddPass("ChromaticAberration", [this, inputHandle, w, h, ca](RenderGraphBuilder& builder)
        {
            builder.DeclarePassCapabilities(RenderCapabilities::RASTER);
            builder.ReadTexture(inputHandle);
            const RenderGraphHandle output = CreatePostProcessOutput(builder, w, h);
            builder.WriteColour(output, LoadOp::DontCare);

            return [this, inputHandle, ca](RenderDevice& device, const RenderGraphResources& resources)
            {
                const ShaderProgram* prog = m_context->GetPrograms().Find("chromatic_aberration");
                const GPUSamplerHandle nearest = m_context->GetNearestSampler();
                const auto inTex = resources.GetTexture(inputHandle);
                if (!prog || !prog->Pipeline.IsValid() || !inTex || !nearest)
                {
                    WAYFINDER_ERROR(LogRenderer, "ChromaticAberration: missing program, pipeline, input, or sampler — skipped");
                    return;
                }

                ChromaticAberrationUBO ubo{};
                ubo.IntensityPad = Float4(ca.Intensity.Value, 0.0f, 0.0f, 0.0f);

                device.BindPipeline(prog->Pipeline);
                device.BindFragmentSampler(0, inTex, nearest);
                device.PushFragmentUniform(0, &ubo, sizeof(ubo));
                device.DrawPrimitives(3);
            };
        });
    }

} // namespace Wayfinder
