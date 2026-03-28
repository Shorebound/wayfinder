#pragma once

#include "rendering/graph/RenderFeature.h"
#include "volumes/BlendableEffectRegistry.h"

namespace Wayfinder
{
    class RenderServices;

    /**
     * @brief Post-process chromatic aberration (radial RGB separation). Self-registers blendable type and shader in OnAttach.
     */
    class ChromaticAberrationFeature final : public RenderFeature
    {
    public:
        std::string_view GetName() const override
        {
            return "ChromaticAberration";
        }

        RenderCapabilityMask GetCapabilities() const override;

        void OnAttach(const RenderFeatureContext& context) override;
        void OnDetach(const RenderFeatureContext& context) override;
        void AddPasses(RenderGraph& graph, const FrameRenderParams& params) override;

    private:
        RenderServices* m_context = nullptr;
        BlendableEffectId m_effectId = INVALID_BLENDABLE_EFFECT_ID;
    };

} // namespace Wayfinder
