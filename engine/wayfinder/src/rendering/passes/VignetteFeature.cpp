#include "VignetteFeature.h"

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

#include <array>

namespace Wayfinder::Rendering
{
    namespace
    {
        /// Matches `vignette.slang` fragment UBO (std140).
        struct alignas(16) VignetteUBO
        {
            float Strength = 0.0f;
            std::array<float, 3> Pad{};
        };
        static_assert(sizeof(VignetteUBO) == 16);
    } // namespace

    RenderCapabilityMask VignetteFeature::GetCapabilities() const
    {
        return RenderCapabilities::RASTER;
    }

    std::span<const ShaderProgramDesc> VignetteFeature::GetShaderPrograms() const
    {
        static const auto PROGRAMS = []
        {
            ShaderProgramDesc desc;
            desc.Name = "vignette";
            desc.VertexShaderName = "vignette";
            desc.FragmentShaderName = "vignette";
            desc.VertexResources = {};
            desc.FragmentResources = {.numUniformBuffers = 1, .numSamplers = 1};
            desc.VertexLayout = VertexLayouts::Empty;
            desc.Cull = CullMode::None;
            desc.DepthTest = false;
            desc.DepthWrite = false;
            desc.MaterialUBOSize = sizeof(VignetteUBO);
            desc.VertexUBOSize = 0;
            desc.NeedsSceneGlobals = false;
            return std::vector{std::move(desc)};
        }();

        return PROGRAMS;
    }

    void VignetteFeature::OnRegisterEffects(BlendableEffectRegistry& registry)
    {
        m_effectId = registry.Register<VignetteParams>("vignette");
        if (m_effectId == INVALID_BLENDABLE_EFFECT_ID)
        {
            WAYFINDER_ERROR(LogRenderer, "VignetteFeature: failed to register vignette blendable type");
        }
    }

    void VignetteFeature::OnAttach(const RenderFeatureContext& context)
    {
        m_context = &context.Context;
    }

    void VignetteFeature::OnDetach(const RenderFeatureContext& /*context*/)
    {
        m_context = nullptr;
        m_effectId = INVALID_BLENDABLE_EFFECT_ID;
    }

    void VignetteFeature::AddPasses(RenderGraph& graph, const FrameRenderParams& params)
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
        const auto vignette = ResolveEffect<VignetteParams>(s, m_effectId);
        if (vignette.Strength.Value <= 0.0f)
        {
            return;
        }

        RenderGraphHandle inputHandle = graph.FindHandle(GraphTextureId::PostProcessColour);
        if (!inputHandle.IsValid())
        {
            inputHandle = graph.FindHandle(GraphTextureId::SceneColour);
        }
        if (!inputHandle.IsValid())
        {
            return; // No upstream colour available — skip vignette.
        }

        const uint32_t w = params.SwapchainWidth;
        const uint32_t h = params.SwapchainHeight;

        graph.AddPass("Vignette", [this, inputHandle, w, h, vignette](RenderGraphBuilder& builder)
        {
            builder.DeclarePassCapabilities(RenderCapabilities::RASTER);
            builder.ReadTexture(inputHandle);
            const RenderGraphHandle output = CreatePostProcessOutput(builder, w, h);
            builder.WriteColour(output, LoadOp::DontCare);

            return [this, inputHandle, vignette](RenderDevice& device, const RenderGraphResources& resources)
            {
                const ShaderProgram* prog = m_context->GetPrograms().Find("vignette");
                const GPUSamplerHandle nearest = m_context->GetNearestSampler();
                const auto inTex = resources.GetTexture(inputHandle);
                if (!prog || !prog->Pipeline.IsValid() || !inTex || !nearest)
                {
                    WAYFINDER_ERROR(LogRenderer, "Vignette: missing program, pipeline, input, or sampler — skipped");
                    return;
                }

                VignetteUBO ubo{};
                ubo.Strength = vignette.Strength.Value;

                device.BindPipeline(prog->Pipeline);
                device.BindFragmentSampler(0, inTex, nearest);
                device.PushFragmentUniform(0, &ubo, sizeof(ubo));
                device.DrawPrimitives(3);
            };
        });
    }

} // namespace Wayfinder::Rendering
