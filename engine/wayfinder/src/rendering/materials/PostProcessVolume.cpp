#include "PostProcessVolume.h"

#include "maths/Maths.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace Wayfinder
{
    namespace
    {
        float ComputeDistanceToVolume(const Wayfinder::PostProcessVolumeInstance& instance, const Wayfinder::Float3& cameraPosition)
        {
            using namespace Wayfinder;

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

            // Box: extract rotation from local-to-world, rotate offset into local
            // orientation, then compute axis-aligned distance with scaled extents.
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

        uint8_t LerpByte(uint8_t a, uint8_t b, float t)
        {
            return static_cast<uint8_t>(Maths::Clamp(static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t, 0.0f, 255.0f));
        }

        Wayfinder::Colour LerpColour(const Wayfinder::Colour& a, const Wayfinder::Colour& b, float t)
        {
            return {.r = LerpByte(a.r, b.r, t), .g = LerpByte(a.g, b.g, t), .b = LerpByte(a.b, b.b, t), .a = LerpByte(a.a, b.a, t)};
        }

        float LerpParamValue(float current, float target, float weight)
        {
            return Maths::Mix(current, target, weight);
        }

        int32_t LerpParamValue(int32_t current, int32_t target, float weight)
        {
            return static_cast<int32_t>(std::round(Maths::Mix(static_cast<float>(current), static_cast<float>(target), weight)));
        }

        Wayfinder::Float3 LerpParamValue(const Wayfinder::Float3& current, const Wayfinder::Float3& target, float weight)
        {
            return Maths::Mix(current, target, weight);
        }

        Wayfinder::Colour LerpParamValue(const Wayfinder::Colour& current, const Wayfinder::Colour& target, float weight)
        {
            return LerpColour(current, target, weight);
        }

        // Blend a single parameter value toward a target by weight.
        Wayfinder::PostProcessParamValue LerpParam(const Wayfinder::PostProcessParamValue& current, const Wayfinder::PostProcessParamValue& target, float weight)
        {
            // Both sides must hold the same type; if mismatched, take the target.
            if (current.index() != target.index())
            {
                return target;
            }

            return std::visit([&](const auto& a) -> Wayfinder::PostProcessParamValue
            {
                using T = std::decay_t<decltype(a)>;
                const auto& b = std::get<T>(target);
                return LerpParamValue(a, b, weight);
            }, current);
        }

        float ZeroParamValue(float)
        {
            return 0.0f;
        }

        int32_t ZeroParamValue(int32_t)
        {
            return int32_t{0};
        }

        Wayfinder::Float3 ZeroParamValue(const Wayfinder::Float3&)
        {
            return Wayfinder::Float3{0.0f, 0.0f, 0.0f};
        }

        Wayfinder::Colour ZeroParamValue(const Wayfinder::Colour&)
        {
            return Wayfinder::Colour{.r = 0, .g = 0, .b = 0, .a = 0};
        }

        Wayfinder::PostProcessParamValue ZeroValue(const Wayfinder::PostProcessParamValue& v)
        {
            return std::visit([](const auto& val) -> Wayfinder::PostProcessParamValue
            {
                return ZeroParamValue(val);
            }, v);
        }

        void BlendEffectInto(Wayfinder::PostProcessEffect& result, const Wayfinder::PostProcessEffect& source, float weight)
        {
            for (const auto& [key, value] : source.Parameters)
            {
                auto it = result.Parameters.find(key);
                if (it != result.Parameters.end())
                {
                    it->second = LerpParam(it->second, value, weight);
                }
                else
                {
                    result.Parameters[key] = LerpParam(ZeroValue(value), value, weight);
                }
            }
        }

    } // namespace
}

namespace Wayfinder
{
    // ── PostProcessEffect accessors ──────────────────────────

    float PostProcessEffect::GetFloat(const std::string_view name, float fallback) const
    {
        auto it = Parameters.find(std::string(name));
        if (it == Parameters.end())
        {
            return fallback;
        }
        if (const auto* v = std::get_if<float>(&it->second))
        {
            return *v;
        }
        if (const auto* v = std::get_if<int32_t>(&it->second))
        {
            return static_cast<float>(*v);
        }
        return fallback;
    }

    int32_t PostProcessEffect::GetInt(const std::string_view name, int32_t fallback) const
    {
        auto it = Parameters.find(std::string(name));
        if (it == Parameters.end())
        {
            return fallback;
        }
        if (const auto* v = std::get_if<int32_t>(&it->second))
        {
            return *v;
        }
        if (const auto* v = std::get_if<float>(&it->second))
        {
            return static_cast<int32_t>(*v);
        }
        return fallback;
    }

    Float3 PostProcessEffect::GetFloat3(const std::string_view name, const Float3& fallback) const
    {
        auto it = Parameters.find(std::string(name));
        if (it == Parameters.end())
        {
            return fallback;
        }
        if (const auto* v = std::get_if<Float3>(&it->second))
        {
            return *v;
        }
        return fallback;
    }

    Colour PostProcessEffect::GetColour(const std::string_view name, const Colour& fallback) const
    {
        auto it = Parameters.find(std::string(name));
        if (it == Parameters.end())
        {
            return fallback;
        }
        if (const auto* v = std::get_if<Colour>(&it->second))
        {
            return *v;
        }
        return fallback;
    }

    // ── PostProcessStack ─────────────────────────────────────

    const PostProcessEffect* PostProcessStack::FindEffect(const std::string_view type) const
    {
        auto it = Effects.find(std::string(type));
        return it != Effects.end() ? &it->second : nullptr;
    }

    bool PostProcessStack::HasEffect(const std::string_view type) const
    {
        return Effects.contains(std::string(type));
    }

    // ── Blending ─────────────────────────────────────────────

    PostProcessStack BlendPostProcessVolumes(const Float3& cameraPosition, std::span<const PostProcessVolumeInstance> volumes)
    {
        PostProcessStack result;

        if (volumes.empty())
        {
            return result;
        }

        // Sort by priority (ascending) — higher priority volumes layer on last and dominate.
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
                if (!effect.Enabled)
                {
                    continue;
                }

                auto it = result.Effects.find(effect.Type);
                if (it != result.Effects.end())
                {
                    BlendEffectInto(it->second, effect, weight);
                }
                else
                {
                    // First contribution — blend from zero baseline so weight is respected.
                    PostProcessEffect& entry = result.Effects[effect.Type];
                    entry.Type = effect.Type;
                    entry.Enabled = true;
                    BlendEffectInto(entry, effect, weight);
                }
            }
        }

        return result;
    }
} // namespace Wayfinder
