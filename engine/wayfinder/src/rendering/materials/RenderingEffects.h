#pragma once

#include "volumes/BlendableEffect.h"
#include "volumes/BlendableEffectRegistry.h"
#include "volumes/Override.h"
#include "volumes/OverrideReflection.h"

#include "core/Types.h"
#include "wayfinder_exports.h"

#include <tuple>

#include <nlohmann/json_fwd.hpp>

namespace Wayfinder
{
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

        static constexpr auto FIELDS = std::make_tuple(FieldDesc{&ColourGradingParams::ExposureStops, std::string_view{"exposure_stops"}}, FieldDesc{&ColourGradingParams::Contrast, std::string_view{"contrast"}},
            FieldDesc{&ColourGradingParams::Saturation, std::string_view{"saturation"}}, FieldDesc{&ColourGradingParams::Lift, std::string_view{"lift"}}, FieldDesc{&ColourGradingParams::Gamma, std::string_view{"gamma"}},
            FieldDesc{&ColourGradingParams::Gain, std::string_view{"gain"}});
    };

    /**
     * @brief Screen-edge darkening strength; consumed by the composition pass when registered.
     */
    struct VignetteParams
    {
        Override<float> Strength{0.0f};

        static constexpr auto FIELDS = std::make_tuple(FieldDesc{&VignetteParams::Strength, std::string_view{"strength"}});
    };

    /**
     * @brief RGB channel separation intensity; consumed by the composition pass when registered.
     */
    struct ChromaticAberrationParams
    {
        Override<float> Intensity{0.0f};

        static constexpr auto FIELDS = std::make_tuple(FieldDesc{&ChromaticAberrationParams::Intensity, std::string_view{"intensity"}});
    };

    /**
     * @brief Explicit trait specialisation for ChromaticAberrationParams: handles legacy
     *        `"chromatic_aberration_intensity"` alt-key on deserialisation.
     */
    template<>
    struct BlendableEffectTraits<ChromaticAberrationParams>
    {
        static ChromaticAberrationParams Identity()
        {
            return ChromaticAberrationParams{};
        }

        static ChromaticAberrationParams Lerp(const ChromaticAberrationParams& current, const ChromaticAberrationParams& source, float weight)
        {
            ChromaticAberrationParams out{};
            LerpFields(out, current, source, weight, ChromaticAberrationParams::FIELDS);
            return out;
        }

        static void Serialise(nlohmann::json& json, const ChromaticAberrationParams& params)
        {
            SerialiseFields(json, params, ChromaticAberrationParams::FIELDS);
        }

        static ChromaticAberrationParams Deserialise(const nlohmann::json& json)
        {
            ChromaticAberrationParams p{};
            DeserialiseFields(json, p, ChromaticAberrationParams::FIELDS);
            if (!p.Intensity.Active)
            {
                ReadOverrideField(json, "chromatic_aberration_intensity", p.Intensity);
            }
            return p;
        }
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
