#include "PostProcessVolume.h"

#include "maths/Maths.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <variant>
#include <vector>

namespace Wayfinder
{
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

        ColourGradingParams IdentityColourGrading()
        {
            return ColourGradingParams{};
        }

        ColourGradingParams LerpColourGrading(const ColourGradingParams& a, const ColourGradingParams& b, float w)
        {
            return ColourGradingParams{
                .ExposureStops = Maths::Mix(a.ExposureStops, b.ExposureStops, w),
                .Contrast = Maths::Mix(a.Contrast, b.Contrast, w),
                .Saturation = Maths::Mix(a.Saturation, b.Saturation, w),
                .Lift = Maths::Mix(a.Lift, b.Lift, w),
                .Gamma = Maths::Mix(a.Gamma, b.Gamma, w),
                .Gain = Maths::Mix(a.Gain, b.Gain, w),
                .VignetteStrength = Maths::Mix(a.VignetteStrength, b.VignetteStrength, w),
                .ChromaticAberrationIntensity = Maths::Mix(a.ChromaticAberrationIntensity, b.ChromaticAberrationIntensity, w),
            };
        }

        BloomParams LerpBloom(const BloomParams& a, const BloomParams& b, float w)
        {
            return BloomParams{
                .Threshold = Maths::Mix(a.Threshold, b.Threshold, w),
                .Intensity = Maths::Mix(a.Intensity, b.Intensity, w),
                .Radius = Maths::Mix(a.Radius, b.Radius, w),
            };
        }

        void BlendPayloadInto(PostProcessEffect& result, const PostProcessEffect& source, float weight)
        {
            if (source.Type != result.Type)
            {
                return;
            }

            if (source.Type == PostProcessEffectType::ColourGrading)
            {
                auto* dst = std::get_if<ColourGradingParams>(&result.Payload);
                const auto* src = std::get_if<ColourGradingParams>(&source.Payload);
                if (dst && src)
                {
                    *dst = LerpColourGrading(*dst, *src, weight);
                }
            }
            else if (source.Type == PostProcessEffectType::Bloom)
            {
                auto* dst = std::get_if<BloomParams>(&result.Payload);
                const auto* src = std::get_if<BloomParams>(&source.Payload);
                if (dst && src)
                {
                    *dst = LerpBloom(*dst, *src, weight);
                }
            }
        }

        void BlendFirstContribution(PostProcessEffect& entry, const PostProcessEffect& source, float weight)
        {
            entry.Type = source.Type;
            entry.Enabled = source.Enabled;

            if (source.Type == PostProcessEffectType::ColourGrading)
            {
                if (const auto* src = std::get_if<ColourGradingParams>(&source.Payload))
                {
                    entry.Payload = LerpColourGrading(IdentityColourGrading(), *src, weight);
                }
            }
            else if (source.Type == PostProcessEffectType::Bloom)
            {
                if (const auto* src = std::get_if<BloomParams>(&source.Payload))
                {
                    entry.Payload = LerpBloom(BloomParams{.Threshold = 1.0f, .Intensity = 0.0f, .Radius = 1.0f}, *src, weight);
                }
            }
        }

    } // namespace

    PostProcessEffectType PostProcessEffect::ParseTypeString(const std::string_view name)
    {
        std::string lower;
        lower.reserve(name.size());
        for (const char c : name)
        {
            lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }

        if (lower == "colour_grading" || lower == "colourgrading" || lower == "grading" || lower == "color_grading")
        {
            return PostProcessEffectType::ColourGrading;
        }
        if (lower == "bloom")
        {
            return PostProcessEffectType::Bloom;
        }
        return PostProcessEffectType::Unknown;
    }

    std::string_view PostProcessEffect::TypeToString(const PostProcessEffectType type)
    {
        switch (type)
        {
        case PostProcessEffectType::ColourGrading:
            return "colour_grading";
        case PostProcessEffectType::Bloom:
            return "bloom";
        case PostProcessEffectType::Unknown:
        default:
            return "unknown";
        }
    }

    const PostProcessEffect* PostProcessStack::FindEffect(const PostProcessEffectType type) const
    {
        if (type == PostProcessEffectType::Unknown)
        {
            return nullptr;
        }
        const auto i = static_cast<size_t>(type);
        if (i >= Effects.size() || !Effects[i].has_value())
        {
            return nullptr;
        }
        return &*Effects[i];
    }

    bool PostProcessStack::HasEffect(const PostProcessEffectType type) const
    {
        return FindEffect(type) != nullptr;
    }

    PostProcessStack BlendPostProcessVolumes(const Float3& cameraPosition, const std::span<const PostProcessVolumeInstance> volumes)
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
                if (!effect.Enabled || effect.Type == PostProcessEffectType::Unknown)
                {
                    continue;
                }

                const auto slot = static_cast<size_t>(effect.Type);
                if (slot >= result.Effects.size())
                {
                    continue;
                }
                auto& slotEffect = result.Effects[slot];
                if (slotEffect.has_value())
                {
                    BlendPayloadInto(*slotEffect, effect, weight);
                }
                else
                {
                    PostProcessEffect entry;
                    BlendFirstContribution(entry, effect, weight);
                    slotEffect = entry;
                }
            }
        }

        return result;
    }

    ColourGradingParams ResolveColourGradingForView(const PostProcessStack& stack)
    {
        const PostProcessEffect* fx = stack.FindEffect(PostProcessEffectType::ColourGrading);
        if (!fx)
        {
            return IdentityColourGrading();
        }
        if (const auto* p = std::get_if<ColourGradingParams>(&fx->Payload))
        {
            return *p;
        }
        return IdentityColourGrading();
    }

} // namespace Wayfinder
