#include "PostProcessVolume.h"

#include "PostProcessRegistry.h"

#include "core/Assert.h"
#include "maths/Maths.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

#include <nlohmann/json.hpp>

static_assert(sizeof(Wayfinder::ColourGradingParams) <= Wayfinder::POST_PROCESS_EFFECT_PAYLOAD_CAPACITY);
static_assert(sizeof(Wayfinder::VignetteParams) <= Wayfinder::POST_PROCESS_EFFECT_PAYLOAD_CAPACITY);
static_assert(sizeof(Wayfinder::ChromaticAberrationParams) <= Wayfinder::POST_PROCESS_EFFECT_PAYLOAD_CAPACITY);

namespace Wayfinder
{
    PostProcessStack::~PostProcessStack()
    {
        if (const PostProcessRegistry* registry = PostProcessRegistry::GetActiveInstance())
        {
            Clear(*registry);
        }
    }

    namespace
    {
        float ComputeDistanceToVolume(const PostProcessVolumeInstance& instance, const Float3& cameraPosition)
        {
            const PostProcessVolumeComponent& volume = *instance.Volume;
            if (volume.Shape == PostProcessVolumeShape::Global)
            {
                return 0.0f;
            }

            const Float3 offset = cameraPosition - instance.WorldPosition;

            if (volume.Shape == PostProcessVolumeShape::Sphere)
            {
                const Float3 absScale = Maths::Abs(instance.WorldScale);
                const float scaledRadius = volume.Radius * Maths::Max(absScale.x, Maths::Max(absScale.y, absScale.z)); // NOLINT(cppcoreguidelines-pro-type-union-access)
                return Maths::Max(Maths::Length(offset) - scaledRadius, 0.0f);
            }

            Matrix3 rotMat(instance.LocalToWorld);
            rotMat[0] = Maths::Normalize(rotMat[0]); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            rotMat[1] = Maths::Normalize(rotMat[1]); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            rotMat[2] = Maths::Normalize(rotMat[2]); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            const Float3 localOffset = Maths::Transpose(rotMat) * offset;

            const Float3 halfExtents = (volume.Dimensions * 0.5f) * Maths::Abs(instance.WorldScale);
            const Float3 absLocal = Maths::Abs(localOffset);
            const Float3 outside = Maths::Max(absLocal - halfExtents, Float3(0.0f));
            return Maths::Length(outside);
        }

        float ComputeBlendWeight(float distance, float blendDistance)
        {
            if (blendDistance <= 0.0f)
            {
                return distance <= 0.0f ? 1.0f : 0.0f;
            }
            return Maths::Clamp(1.0f - (distance / blendDistance), 0.0f, 1.0f);
        }

    } // namespace

    void PostProcessEffect::DestroyPayload(const PostProcessRegistry& registry)
    {
        if (TypeId == INVALID_POST_PROCESS_EFFECT_ID)
        {
            return;
        }
        const PostProcessEffectDesc* desc = registry.Find(TypeId);
        if (desc == nullptr || desc->Destroy == nullptr)
        {
            return;
        }
        // Type-erased payload buffer; registry Destroy runs the correct destructor for TypeId.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        desc->Destroy(Payload);
    }

    const PostProcessEffect* PostProcessStack::FindEffect(const PostProcessEffectId id) const
    {
        const auto it = std::ranges::find_if(Effects, [id](const PostProcessEffect& e)
        {
            return e.TypeId == id;
        });
        if (it == Effects.end())
        {
            return nullptr;
        }
        return &*it;
    }

    PostProcessEffect* PostProcessStack::FindEffectMutable(const PostProcessEffectId id)
    {
        const auto it = std::ranges::find_if(Effects, [id](const PostProcessEffect& e)
        {
            return e.TypeId == id;
        });
        if (it == Effects.end())
        {
            return nullptr;
        }
        return &*it;
    }

    PostProcessEffect& PostProcessStack::GetOrCreate(const PostProcessEffectId id, const PostProcessRegistry& registry)
    {
        if (PostProcessEffect* existing = FindEffectMutable(id))
        {
            return *existing;
        }

        const PostProcessEffectDesc* desc = registry.Find(id);
        // NOLINTBEGIN(readability-simplify-boolean-expr)
        WAYFINDER_ASSERT(desc != nullptr);
        WAYFINDER_ASSERT(desc->CreateIdentity != nullptr);
        // NOLINTEND(readability-simplify-boolean-expr)

        PostProcessEffect effect{};
        effect.TypeId = id;
        effect.Enabled = true;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        desc->CreateIdentity(effect.Payload);
        Effects.push_back(effect);
        return Effects.back();
    }

    void PostProcessStack::Clear(const PostProcessRegistry& registry)
    {
        for (PostProcessEffect& e : Effects)
        {
            e.DestroyPayload(registry);
        }
        Effects.clear();
    }

    ColourGradingParams Identity(PostProcessTag<ColourGradingParams>)
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

    ColourGradingParams Deserialise(PostProcessTag<ColourGradingParams>, const nlohmann::json& json)
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

    VignetteParams Identity(PostProcessTag<VignetteParams>)
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

    VignetteParams Deserialise(PostProcessTag<VignetteParams>, const nlohmann::json& json)
    {
        VignetteParams p{};
        if (auto it = json.find("strength"); it != json.end() && it->is_number())
        {
            p.Strength = Override<float>::Set(it->get<float>());
        }
        return p;
    }

    ChromaticAberrationParams Identity(PostProcessTag<ChromaticAberrationParams>)
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

    ChromaticAberrationParams Deserialise(PostProcessTag<ChromaticAberrationParams>, const nlohmann::json& json)
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

    PostProcessStack BlendPostProcessVolumes(const Float3& cameraPosition, const std::span<const PostProcessVolumeInstance> volumes, const PostProcessRegistry& registry)
    {
        PostProcessStack result;
        if (volumes.empty())
        {
            return result;
        }

        std::vector<const PostProcessVolumeInstance*> sorted;
        sorted.reserve(volumes.size());
        for (const auto& instance : volumes)
        {
            if (!instance.Volume)
            {
                continue;
            }
            sorted.push_back(&instance);
        }

        std::ranges::stable_sort(sorted, [](const auto* a, const auto* b)
        {
            return a->Volume->Priority < b->Volume->Priority;
        });

        for (const auto* instance : sorted)
        {
            const float distance = ComputeDistanceToVolume(*instance, cameraPosition);
            const float weight = ComputeBlendWeight(distance, instance->Volume->BlendDistance);
            if (weight <= 0.0f)
            {
                continue;
            }

            for (const auto& effect : instance->Volume->Effects)
            {
                if (!effect.Enabled || effect.TypeId == INVALID_POST_PROCESS_EFFECT_ID)
                {
                    continue;
                }

                const PostProcessEffectDesc* desc = registry.Find(effect.TypeId);
                if (desc == nullptr || desc->Blend == nullptr)
                {
                    continue;
                }

                PostProcessEffect& slot = result.GetOrCreate(effect.TypeId, registry);
                // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
                desc->Blend(slot.Payload, effect.Payload, weight);
                // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
            }
        }

        return result;
    }

    ColourGradingParams ResolveColourGradingForView(const PostProcessStack& stack, const PostProcessEffectId id)
    {
        const auto* p = stack.FindPayload<ColourGradingParams>(id);
        if (p == nullptr)
        {
            return Identity(PostProcessTag<ColourGradingParams>{});
        }
        return *p;
    }

    VignetteParams ResolveVignetteForView(const PostProcessStack& stack, const PostProcessEffectId id)
    {
        const auto* p = stack.FindPayload<VignetteParams>(id);
        if (p == nullptr)
        {
            return Identity(PostProcessTag<VignetteParams>{});
        }
        return *p;
    }

    ChromaticAberrationParams ResolveChromaticAberrationForView(const PostProcessStack& stack, const PostProcessEffectId id)
    {
        const auto* p = stack.FindPayload<ChromaticAberrationParams>(id);
        if (p == nullptr)
        {
            return Identity(PostProcessTag<ChromaticAberrationParams>{});
        }
        return *p;
    }

    std::string NormalisePostProcessEffectTypeString(const std::string_view name)
    {
        std::string lower;
        lower.reserve(name.size());
        for (const char c : name)
        {
            lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        return lower;
    }

    bool IsValidPostProcessEffectTypeName(const std::string_view normalisedLower)
    {
        if (const PostProcessRegistry* reg = PostProcessRegistry::GetActiveInstance())
        {
            return reg->FindIdByName(normalisedLower).has_value();
        }
        return normalisedLower == "colour_grading" || normalisedLower == "vignette" || normalisedLower == "chromatic_aberration";
    }

} // namespace Wayfinder
