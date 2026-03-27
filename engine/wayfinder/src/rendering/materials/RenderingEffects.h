#pragma once

#include "volumes/BlendableEffect.h"
#include "volumes/BlendableEffectRegistry.h"
#include "volumes/Override.h"

#include "core/Types.h"
#include "wayfinder_exports.h"

#include <nlohmann/json_fwd.hpp>

namespace Wayfinder
{
    struct ColourGradingParams;
    struct VignetteParams;
    struct ChromaticAberrationParams;

    /**
     * @brief ADL hooks for blendable effect payloads. `BlendableEffectRegistry` resolves these via
     * argument-dependent lookup in this namespace; each payload type supplies Identity, Lerp,
     * Serialise, and Deserialise.
     */

    /** @{ ColourGradingParams — ADL hooks */

    /**
     * @brief Returns the identity (default) colour grading parameters for blending and composition.
     * @param tag Effect type tag used only for overload resolution.
     * @return Default parameters with inactive overrides.
     */
    [[nodiscard]] ColourGradingParams Identity(EffectTag<ColourGradingParams>);
    /**
     * @brief Linearly blends two colour grading parameter sets using per-field override rules.
     * @param current Accumulated parameters from the blend stack.
     * @param source Parameters from the next contributing effect.
     * @param weight Blend weight in [0, 1] for the source contribution.
     * @return Blended colour grading parameters.
     */
    [[nodiscard]] ColourGradingParams Lerp(const ColourGradingParams& current, const ColourGradingParams& source, float weight);
    /**
     * @brief Writes active colour grading fields to JSON for scene serialisation.
     * @param json JSON object to receive the serialised fields.
     * @param params Colour grading parameters to write.
     */
    void Serialise(nlohmann::json& json, const ColourGradingParams& params);
    /**
     * @brief Parses colour grading parameters from JSON for scene load.
     * @param tag Effect type tag used only for overload resolution.
     * @param json JSON object containing effect fields (may be partial).
     * @return Deserialised colour grading parameters.
     */
    [[nodiscard]] ColourGradingParams Deserialise(EffectTag<ColourGradingParams>, const nlohmann::json& json);

    /** @} */

    /** @{ VignetteParams — ADL hooks */

    /**
     * @brief Returns the identity (default) vignette parameters for blending and composition.
     * @param tag Effect type tag used only for overload resolution.
     * @return Default parameters with inactive overrides.
     */
    [[nodiscard]] VignetteParams Identity(EffectTag<VignetteParams>);
    /**
     * @brief Linearly blends two vignette parameter sets using per-field override rules.
     * @param current Accumulated parameters from the blend stack.
     * @param source Parameters from the next contributing effect.
     * @param weight Blend weight in [0, 1] for the source contribution.
     * @return Blended vignette parameters.
     */
    [[nodiscard]] VignetteParams Lerp(const VignetteParams& current, const VignetteParams& source, float weight);
    /**
     * @brief Writes active vignette fields to JSON for scene serialisation.
     * @param json JSON object to receive the serialised fields.
     * @param params Vignette parameters to write.
     */
    void Serialise(nlohmann::json& json, const VignetteParams& params);
    /**
     * @brief Parses vignette parameters from JSON for scene load.
     * @param tag Effect type tag used only for overload resolution.
     * @param json JSON object containing effect fields (may be partial).
     * @return Deserialised vignette parameters.
     */
    [[nodiscard]] VignetteParams Deserialise(EffectTag<VignetteParams>, const nlohmann::json& json);

    /** @} */

    /** @{ ChromaticAberrationParams — ADL hooks */

    /**
     * @brief Returns the identity (default) chromatic aberration parameters for blending and composition.
     * @param tag Effect type tag used only for overload resolution.
     * @return Default parameters with inactive overrides.
     */
    [[nodiscard]] ChromaticAberrationParams Identity(EffectTag<ChromaticAberrationParams>);
    /**
     * @brief Linearly blends two chromatic aberration parameter sets using per-field override rules.
     * @param current Accumulated parameters from the blend stack.
     * @param source Parameters from the next contributing effect.
     * @param weight Blend weight in [0, 1] for the source contribution.
     * @return Blended chromatic aberration parameters.
     */
    [[nodiscard]] ChromaticAberrationParams Lerp(const ChromaticAberrationParams& current, const ChromaticAberrationParams& source, float weight);
    /**
     * @brief Writes active chromatic aberration fields to JSON for scene serialisation.
     * @param json JSON object to receive the serialised fields.
     * @param params Chromatic aberration parameters to write.
     */
    void Serialise(nlohmann::json& json, const ChromaticAberrationParams& params);
    /**
     * @brief Parses chromatic aberration parameters from JSON for scene load.
     * @param tag Effect type tag used only for overload resolution.
     * @param json JSON object containing effect fields (may be partial).
     * @return Deserialised chromatic aberration parameters.
     */
    [[nodiscard]] ChromaticAberrationParams Deserialise(EffectTag<ChromaticAberrationParams>, const nlohmann::json& json);

    /** @} */

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

    /**
     * @brief Screen-edge darkening strength; consumed by the composition pass when registered.
     */
    struct VignetteParams
    {
        Override<float> Strength{0.0f};
    };

    /**
     * @brief RGB channel separation intensity; consumed by the composition pass when registered.
     */
    struct ChromaticAberrationParams
    {
        Override<float> Intensity{0.0f};
    };

    /**
     * @brief Resolves colour grading parameters for a view from the blended stack.
     * @param stack Blended effect stack for the view.
     * @param id Engine-registered colour grading effect id.
     * @return Resolved parameters, or identity values if the payload is missing.
     */
    [[nodiscard]] WAYFINDER_API ColourGradingParams ResolveColourGradingForView(const BlendableEffectStack& stack, BlendableEffectId id);
    /**
     * @brief Resolves vignette parameters for a view from the blended stack.
     * @param stack Blended effect stack for the view.
     * @param id Engine-registered vignette effect id.
     * @return Resolved parameters, or identity values if the payload is missing.
     */
    [[nodiscard]] WAYFINDER_API VignetteParams ResolveVignetteForView(const BlendableEffectStack& stack, BlendableEffectId id);
    /**
     * @brief Resolves chromatic aberration parameters for a view from the blended stack.
     * @param stack Blended effect stack for the view.
     * @param id Engine-registered chromatic aberration effect id.
     * @return Resolved parameters, or identity values if the payload is missing.
     */
    [[nodiscard]] WAYFINDER_API ChromaticAberrationParams ResolveChromaticAberrationForView(const BlendableEffectStack& stack, BlendableEffectId id);

} // namespace Wayfinder
