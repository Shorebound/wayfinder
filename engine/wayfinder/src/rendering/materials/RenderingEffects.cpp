#include "RenderingEffects.h"

#include "volumes/BlendableEffectRegistry.h"
#include "volumes/Override.h"

#include <nlohmann/json.hpp>

static_assert(sizeof(Wayfinder::ColourGradingParams) <= Wayfinder::BLENDABLE_EFFECT_PAYLOAD_CAPACITY);
static_assert(sizeof(Wayfinder::VignetteParams) <= Wayfinder::BLENDABLE_EFFECT_PAYLOAD_CAPACITY);
static_assert(sizeof(Wayfinder::ChromaticAberrationParams) <= Wayfinder::BLENDABLE_EFFECT_PAYLOAD_CAPACITY);

namespace Wayfinder
{
    // ── ColourGradingParams ─────────────────────────────────────────────────

    ColourGradingParams Identity(EffectTag<ColourGradingParams>)
    {
        return ColourGradingParams{};
    }

    ColourGradingParams Lerp(const ColourGradingParams& current, const ColourGradingParams& source, const float weight)
    {
        ColourGradingParams out{};
        out.ExposureStops = LerpOverride(current.ExposureStops, source.ExposureStops, weight);
        out.Contrast = LerpOverride(current.Contrast, source.Contrast, weight);
        out.Saturation = LerpOverride(current.Saturation, source.Saturation, weight);
        out.Lift = LerpOverride(current.Lift, source.Lift, weight);
        out.Gamma = LerpOverride(current.Gamma, source.Gamma, weight);
        out.Gain = LerpOverride(current.Gain, source.Gain, weight);
        return out;
    }

    void Serialise(nlohmann::json& json, const ColourGradingParams& params)
    {
        if (params.ExposureStops.Active)
        {
            json["exposure_stops"] = params.ExposureStops.Value;
        }
        if (params.Contrast.Active)
        {
            json["contrast"] = params.Contrast.Value;
        }
        if (params.Saturation.Active)
        {
            json["saturation"] = params.Saturation.Value;
        }
        if (params.Lift.Active)
        {
            json["lift"] = nlohmann::json::array({params.Lift.Value.x, params.Lift.Value.y, params.Lift.Value.z});
        }
        if (params.Gamma.Active)
        {
            json["gamma"] = nlohmann::json::array({params.Gamma.Value.x, params.Gamma.Value.y, params.Gamma.Value.z});
        }
        if (params.Gain.Active)
        {
            json["gain"] = nlohmann::json::array({params.Gain.Value.x, params.Gain.Value.y, params.Gain.Value.z});
        }
    }

    ColourGradingParams Deserialise(EffectTag<ColourGradingParams>, const nlohmann::json& json)
    {
        ColourGradingParams p{};
        if (auto it = json.find("exposure_stops"); it != json.end() && it->is_number())
        {
            p.ExposureStops = Override<float>::Set(it->get<float>());
        }
        if (auto it = json.find("contrast"); it != json.end() && it->is_number())
        {
            p.Contrast = Override<float>::Set(it->get<float>());
        }
        if (auto it = json.find("saturation"); it != json.end() && it->is_number())
        {
            p.Saturation = Override<float>::Set(it->get<float>());
        }
        if (auto it = json.find("lift"); it != json.end() && it->is_array() && it->size() >= 3)
        {
            p.Lift = Override<Float3>::Set(Float3{(*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>()});
        }
        if (auto it = json.find("gamma"); it != json.end() && it->is_array() && it->size() >= 3)
        {
            p.Gamma = Override<Float3>::Set(Float3{(*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>()});
        }
        if (auto it = json.find("gain"); it != json.end() && it->is_array() && it->size() >= 3)
        {
            p.Gain = Override<Float3>::Set(Float3{(*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>()});
        }
        return p;
    }

    // ── VignetteParams ──────────────────────────────────────────────────────

    VignetteParams Identity(EffectTag<VignetteParams>)
    {
        return VignetteParams{};
    }

    VignetteParams Lerp(const VignetteParams& current, const VignetteParams& source, const float weight)
    {
        VignetteParams out{};
        out.Strength = LerpOverride(current.Strength, source.Strength, weight);
        return out;
    }

    void Serialise(nlohmann::json& json, const VignetteParams& params)
    {
        if (params.Strength.Active)
        {
            json["strength"] = params.Strength.Value;
        }
    }

    VignetteParams Deserialise(EffectTag<VignetteParams>, const nlohmann::json& json)
    {
        VignetteParams p{};
        if (auto it = json.find("strength"); it != json.end() && it->is_number())
        {
            p.Strength = Override<float>::Set(it->get<float>());
        }
        return p;
    }

    // ── ChromaticAberrationParams ───────────────────────────────────────────

    ChromaticAberrationParams Identity(EffectTag<ChromaticAberrationParams>)
    {
        return ChromaticAberrationParams{};
    }

    ChromaticAberrationParams Lerp(const ChromaticAberrationParams& current, const ChromaticAberrationParams& source, const float weight)
    {
        ChromaticAberrationParams out{};
        out.Intensity = LerpOverride(current.Intensity, source.Intensity, weight);
        return out;
    }

    void Serialise(nlohmann::json& json, const ChromaticAberrationParams& params)
    {
        if (params.Intensity.Active)
        {
            json["intensity"] = params.Intensity.Value;
        }
    }

    ChromaticAberrationParams Deserialise(EffectTag<ChromaticAberrationParams>, const nlohmann::json& json)
    {
        ChromaticAberrationParams p{};
        if (auto it = json.find("intensity"); it != json.end() && it->is_number())
        {
            p.Intensity = Override<float>::Set(it->get<float>());
        }
        else if (auto it = json.find("chromatic_aberration_intensity"); it != json.end() && it->is_number())
        {
            p.Intensity = Override<float>::Set(it->get<float>());
        }
        return p;
    }

    // ── Resolve helpers ─────────────────────────────────────────────────────

    ColourGradingParams ResolveColourGradingForView(const BlendableEffectStack& stack, const BlendableEffectId id)
    {
        const auto* p = stack.FindPayload<ColourGradingParams>(id);
        if (p == nullptr)
        {
            return Identity(EffectTag<ColourGradingParams>{});
        }
        return *p;
    }

    VignetteParams ResolveVignetteForView(const BlendableEffectStack& stack, const BlendableEffectId id)
    {
        const auto* p = stack.FindPayload<VignetteParams>(id);
        if (p == nullptr)
        {
            return Identity(EffectTag<VignetteParams>{});
        }
        return *p;
    }

    ChromaticAberrationParams ResolveChromaticAberrationForView(const BlendableEffectStack& stack, const BlendableEffectId id)
    {
        const auto* p = stack.FindPayload<ChromaticAberrationParams>(id);
        if (p == nullptr)
        {
            return Identity(EffectTag<ChromaticAberrationParams>{});
        }
        return *p;
    }

} // namespace Wayfinder
