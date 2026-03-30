#include "ColourGradingFeature.h"

#include "rendering/backend/VertexFormats.h"
#include "rendering/graph/RenderCapabilities.h"
#include "rendering/graph/RenderGraph.h"
#include "rendering/materials/RenderingEffects.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/pipeline/RenderServices.h"
#include "volumes/BlendableEffect.h"
#include "volumes/BlendableEffectTraits.h"
#include "volumes/BlendableEffectUtils.h"

#include "core/Log.h"
#include "core/Types.h"

#ifdef WAYFINDER_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4324) // lambda closure padded due to captured alignas(16) UBO
#endif

namespace Wayfinder
{
    namespace
    {
        /// Matches `colour_grading.slang` fragment UBO (std140).
        struct alignas(16) ColourGradingUBO
        {
            Float4 ExposureContrastSaturation{};
            Float4 Lift{};
            Float4 Gamma{};
            Float4 Gain{};
        };
        static_assert(sizeof(ColourGradingUBO) == 64);

        [[nodiscard]] ColourGradingUBO BuildColourGradingUBO(const ColourGradingParams& grading)
        {
            ColourGradingUBO u{};
            u.ExposureContrastSaturation = Float4(grading.ExposureStops.Value, grading.Contrast.Value, grading.Saturation.Value, 0.0f);
            u.Lift = Float4(grading.Lift.Value.x, grading.Lift.Value.y, grading.Lift.Value.z, 0.0f);
            u.Gamma = Float4(grading.Gamma.Value.x, grading.Gamma.Value.y, grading.Gamma.Value.z, 0.0f);
            u.Gain = Float4(grading.Gain.Value.x, grading.Gain.Value.y, grading.Gain.Value.z, 0.0f);
            return u;
        }
    } // namespace

    RenderCapabilityMask ColourGradingFeature::GetCapabilities() const
    {
        return RenderCapabilities::RASTER;
    }

    std::span<const ShaderProgramDesc> ColourGradingFeature::GetShaderPrograms() const
    {
        static const auto PROGRAMS = []
        {
            ShaderProgramDesc desc;
            desc.Name = "colour_grading";
            desc.VertexShaderName = "colour_grading";
            desc.FragmentShaderName = "colour_grading";
            desc.VertexResources = {};
            desc.FragmentResources = {.UniformBuffers = 1, .Samplers = 1};
            desc.VertexLayout = VertexLayouts::EMPTY;
            desc.Cull = CullMode::None;
            desc.DepthTest = false;
            desc.DepthWrite = false;
            desc.MaterialUBOSize = sizeof(ColourGradingUBO);
            desc.VertexUBOSize = 0;
            desc.NeedsSceneGlobals = false;
            return std::vector{std::move(desc)};
        }();

        return PROGRAMS;
    }

    void ColourGradingFeature::OnRegisterEffects(BlendableEffectRegistry& registry)
    {
        m_effectId = registry.Register<ColourGradingParams>("colour_grading");
        if (m_effectId == INVALID_BLENDABLE_EFFECT_ID)
        {
            Log::Error(LogRenderer, "ColourGradingFeature: failed to register colour_grading blendable type");
        }
    }

    void ColourGradingFeature::OnAttach(const RenderFeatureContext& context)
    {
        m_context = &context.Context;
    }

    void ColourGradingFeature::OnDetach(const RenderFeatureContext& /*context*/)
    {
        m_context = nullptr;
        m_effectId = INVALID_BLENDABLE_EFFECT_ID;
    }

    void ColourGradingFeature::AddPasses(RenderGraph& graph, const FrameRenderParams& params)
    {
        if (!m_context)
        {
            return;
        }

        ColourGradingParams grading = BlendableEffectTraits<ColourGradingParams>::Identity();
        if (m_effectId != INVALID_BLENDABLE_EFFECT_ID)
        {
            const BlendableEffectStack* stack = nullptr;
            if (params.PrimaryView.Valid && params.PrimaryView.ViewIndex < params.Frame.Views.size())
            {
                stack = &params.Frame.Views[params.PrimaryView.ViewIndex].PostProcess;
            }
            const BlendableEffectStack emptyStack{};
            const BlendableEffectStack& s = stack ? *stack : emptyStack;
            grading = ResolveEffect<ColourGradingParams>(s, m_effectId);
        }
        const ColourGradingUBO ubo = BuildColourGradingUBO(grading);

        const RenderGraphHandle inputHandle = ResolvePostProcessInput(graph);
        const uint32_t w = params.SwapchainWidth;
        const uint32_t h = params.SwapchainHeight;

        graph.AddPass("ColourGrading", [this, inputHandle, w, h, ubo](RenderGraphBuilder& builder)
        {
            builder.DeclarePassCapabilities(RenderCapabilities::RASTER);
            builder.ReadTexture(inputHandle);
            const RenderGraphHandle output = CreatePostProcessOutput(builder, w, h);
            builder.WriteColour(output, LoadOp::DontCare);

            return [this, inputHandle, ubo](RenderDevice& device, const RenderGraphResources& resources)
            {
                const ShaderProgram* prog = m_context->GetPrograms().Find("colour_grading");
                const GPUSamplerHandle nearest = m_context->GetNearestSampler();
                const auto inTex = resources.GetTexture(inputHandle);
                if (!prog || !prog->Pipeline.IsValid() || !inTex || !nearest)
                {
                    Log::Error(LogRenderer, "ColourGrading: missing program, pipeline, input, or sampler — skipped");
                    return;
                }

                device.BindPipeline(prog->Pipeline);
                device.BindFragmentSampler(0, inTex, nearest);
                device.PushFragmentUniform(0, &ubo, sizeof(ubo));
                device.DrawPrimitives(3);
            };
        });
    }

#ifdef WAYFINDER_COMPILER_MSVC
#pragma warning(pop)
#endif

} // namespace Wayfinder
