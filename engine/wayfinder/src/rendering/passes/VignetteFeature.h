#pragma once

#include "rendering/graph/RenderFeature.h"
#include "volumes/BlendableEffectRegistry.h"

namespace Wayfinder
{
    class RenderServices;
}

namespace Wayfinder::Rendering
{
    /**
     * @brief Post-process vignette (edge darkening).
     *
     * Shader programs are exposed via GetShaderPrograms() and effect/blendable
     * registration happens in OnRegisterEffects().
     */
    class VignetteFeature final : public RenderFeature
    {
    public:
        std::string_view GetName() const override
        {
            return "Vignette";
        }

        RenderCapabilityMask GetCapabilities() const override;

        std::vector<ShaderProgramDesc> GetShaderPrograms() const override;
        void OnRegisterEffects(BlendableEffectRegistry& registry) override;
        void OnAttach(const RenderFeatureContext& context) override;
        void OnDetach(const RenderFeatureContext& context) override;
        void AddPasses(RenderGraph& graph, const FrameRenderParams& params) override;

    private:
        RenderServices* m_context = nullptr;
        BlendableEffectId m_effectId = INVALID_BLENDABLE_EFFECT_ID;
    };

} // namespace Wayfinder::Rendering
