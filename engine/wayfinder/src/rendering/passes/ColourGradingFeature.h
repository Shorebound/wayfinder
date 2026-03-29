#pragma once

#include "rendering/graph/RenderFeature.h"
#include "volumes/BlendableEffectRegistry.h"

namespace Wayfinder
{
    class RenderServices;

    /**
     * @brief Tonemapping and colour grading (lift/gamma/gain, contrast, saturation). Self-registers blendable type and shader in OnAttach.
     */
    class ColourGradingFeature final : public RenderFeature
    {
    public:
        std::string_view GetName() const override
        {
            return "ColourGrading";
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

} // namespace Wayfinder
