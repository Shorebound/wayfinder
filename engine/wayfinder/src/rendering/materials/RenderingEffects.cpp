#include "RenderingEffects.h"

#include "volumes/BlendableEffectRegistry.h"

static_assert(sizeof(Wayfinder::ColourGradingParams) <= Wayfinder::BLENDABLE_EFFECT_PAYLOAD_CAPACITY);
static_assert(sizeof(Wayfinder::VignetteParams) <= Wayfinder::BLENDABLE_EFFECT_PAYLOAD_CAPACITY);
static_assert(sizeof(Wayfinder::ChromaticAberrationParams) <= Wayfinder::BLENDABLE_EFFECT_PAYLOAD_CAPACITY);

namespace Wayfinder
{
    // ── Resolve helpers ─────────────────────────────────────────────────────

    ColourGradingParams ResolveColourGradingForView(const BlendableEffectStack& stack, const BlendableEffectId id)
    {
        const auto* p = stack.FindPayload<ColourGradingParams>(id);
        return p ? *p : ColourGradingParams{};
    }

    VignetteParams ResolveVignetteForView(const BlendableEffectStack& stack, const BlendableEffectId id)
    {
        const auto* p = stack.FindPayload<VignetteParams>(id);
        return p ? *p : VignetteParams{};
    }

    ChromaticAberrationParams ResolveChromaticAberrationForView(const BlendableEffectStack& stack, const BlendableEffectId id)
    {
        const auto* p = stack.FindPayload<ChromaticAberrationParams>(id);
        return p ? *p : ChromaticAberrationParams{};
    }

} // namespace Wayfinder
