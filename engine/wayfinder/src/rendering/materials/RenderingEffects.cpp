#include "RenderingEffects.h"

#include "volumes/BlendableEffectRegistry.h"

static_assert(sizeof(Wayfinder::ColourGradingParams) <= Wayfinder::BLENDABLE_EFFECT_PAYLOAD_CAPACITY);
static_assert(sizeof(Wayfinder::VignetteParams) <= Wayfinder::BLENDABLE_EFFECT_PAYLOAD_CAPACITY);
static_assert(sizeof(Wayfinder::ChromaticAberrationParams) <= Wayfinder::BLENDABLE_EFFECT_PAYLOAD_CAPACITY);

namespace Wayfinder
{} // namespace Wayfinder
