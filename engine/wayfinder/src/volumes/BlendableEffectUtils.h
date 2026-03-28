#pragma once

#include "volumes/BlendableEffect.h"
#include "volumes/BlendableEffectRegistry.h"
#include "volumes/BlendableEffectTraits.h"

namespace Wayfinder
{
    /**
     * @brief Resolve a blendable effect payload from the stack, or return identity if absent.
     */
    template<BlendableEffectPayload TPayload>
    [[nodiscard]] TPayload ResolveEffect(const BlendableEffectStack& stack, BlendableEffectId id)
    {
        const TPayload* p = stack.FindPayload<TPayload>(id);
        return p ? *p : BlendableEffectTraits<TPayload>::Identity();
    }

} // namespace Wayfinder
