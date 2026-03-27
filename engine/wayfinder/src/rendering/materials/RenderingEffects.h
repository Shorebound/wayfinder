#pragma once

#include "volumes/Override.h"
#include "volumes/VolumeEffect.h"
#include "volumes/VolumeEffectRegistry.h"

#include "core/Types.h"
#include "wayfinder_exports.h"

#include <nlohmann/json_fwd.hpp>

namespace Wayfinder
{
    struct ColourGradingParams;
    struct VignetteParams;
    struct ChromaticAberrationParams;

    // ── ADL hooks (BlendableEffect) ────────────────────────────────────────

    [[nodiscard]] ColourGradingParams Identity(EffectTag<ColourGradingParams>);
    [[nodiscard]] ColourGradingParams Lerp(const ColourGradingParams& current, const ColourGradingParams& source, float weight);
    void Serialise(nlohmann::json& json, const ColourGradingParams& params);
    [[nodiscard]] ColourGradingParams Deserialise(EffectTag<ColourGradingParams>, const nlohmann::json& json);

    [[nodiscard]] VignetteParams Identity(EffectTag<VignetteParams>);
    [[nodiscard]] VignetteParams Lerp(const VignetteParams& current, const VignetteParams& source, float weight);
    void Serialise(nlohmann::json& json, const VignetteParams& params);
    [[nodiscard]] VignetteParams Deserialise(EffectTag<VignetteParams>, const nlohmann::json& json);

    [[nodiscard]] ChromaticAberrationParams Identity(EffectTag<ChromaticAberrationParams>);
    [[nodiscard]] ChromaticAberrationParams Lerp(const ChromaticAberrationParams& current, const ChromaticAberrationParams& source, float weight);
    void Serialise(nlohmann::json& json, const ChromaticAberrationParams& params);
    [[nodiscard]] ChromaticAberrationParams Deserialise(EffectTag<ChromaticAberrationParams>, const nlohmann::json& json);

    /**
     * @brief Final-screen colour grading (lift / gamma / gain, exposure, contrast, saturation).
     * Vignette and chromatic aberration are separate effect types.
     */
    struct ColourGradingParams
    {
        Override<float> ExposureStops{0.0f};
        Override<float> Contrast{1.0f};
        Override<float> Saturation{1.0f};
        Override<Float3> Lift{{0.0f, 0.0f, 0.0f}};
        Override<Float3> Gamma{{1.0f, 1.0f, 1.0f}};
        Override<Float3> Gain{{1.0f, 1.0f, 1.0f}};
    };

    struct VignetteParams
    {
        Override<float> Strength{0.0f};
    };

    struct ChromaticAberrationParams
    {
        Override<float> Intensity{0.0f};
    };

    [[nodiscard]] WAYFINDER_API ColourGradingParams ResolveColourGradingForView(const VolumeEffectStack& stack, VolumeEffectId id);
    [[nodiscard]] WAYFINDER_API VignetteParams ResolveVignetteForView(const VolumeEffectStack& stack, VolumeEffectId id);
    [[nodiscard]] WAYFINDER_API ChromaticAberrationParams ResolveChromaticAberrationForView(const VolumeEffectStack& stack, VolumeEffectId id);

} // namespace Wayfinder
