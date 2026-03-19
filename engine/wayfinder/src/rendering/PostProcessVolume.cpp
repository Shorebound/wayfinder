#include "PostProcessVolume.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>

namespace
{
    float ComputeDistanceToVolume(
        const Wayfinder::PostProcessVolumeComponent& volume,
        const Wayfinder::Float3& worldPosition,
        const Wayfinder::Float3& worldScale,
        const Wayfinder::Matrix4& localToWorld,
        const Wayfinder::Float3& cameraPosition)
    {
        using namespace Wayfinder;

        if (volume.Shape == PostProcessVolumeShape::Global) { return 0.0f; }

        const Float3 offset = cameraPosition - worldPosition;

        if (volume.Shape == PostProcessVolumeShape::Sphere)
        {
            const Float3 absScale = glm::abs(worldScale);
            const float scaledRadius = volume.Radius * glm::max(absScale.x, glm::max(absScale.y, absScale.z));
            return glm::max(glm::length(offset) - scaledRadius, 0.0f);
        }

        // Box: extract rotation from local-to-world, rotate offset into local
        // orientation, then compute axis-aligned distance with scaled extents.
        glm::mat3 rotMat(localToWorld);
        rotMat[0] = glm::normalize(rotMat[0]);
        rotMat[1] = glm::normalize(rotMat[1]);
        rotMat[2] = glm::normalize(rotMat[2]);
        const Float3 localOffset = glm::transpose(rotMat) * offset;

        const Float3 halfExtents = (volume.Dimensions * 0.5f) * glm::abs(worldScale);
        const Float3 absLocal = glm::abs(localOffset);
        const Float3 outside = glm::max(absLocal - halfExtents, Float3(0.0f));
        return glm::length(outside);
    }

    float ComputeBlendWeight(float distance, float blendDistance)
    {
        if (blendDistance <= 0.0f) { return distance <= 0.0f ? 1.0f : 0.0f; }
        return glm::clamp(1.0f - (distance / blendDistance), 0.0f, 1.0f);
    }

    uint8_t LerpByte(uint8_t a, uint8_t b, float t)
    {
        return static_cast<uint8_t>(glm::clamp(
            static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t,
            0.0f, 255.0f));
    }

    Wayfinder::Color LerpColor(const Wayfinder::Color& a, const Wayfinder::Color& b, float t)
    {
        return {
            .r = LerpByte(a.r, b.r, t),
            .g = LerpByte(a.g, b.g, t),
            .b = LerpByte(a.b, b.b, t),
            .a = LerpByte(a.a, b.a, t)};
    }

    // Blend a single parameter value toward a target by weight.
    Wayfinder::PostProcessParamValue LerpParam(
        const Wayfinder::PostProcessParamValue& current,
        const Wayfinder::PostProcessParamValue& target,
        float weight)
    {
        // Both sides must hold the same type; if mismatched, take the target.
        if (current.index() != target.index()) { return target; }

        return std::visit([&](const auto& a) -> Wayfinder::PostProcessParamValue
        {
            using T = std::decay_t<decltype(a)>;
            const auto& b = std::get<T>(target);

            if constexpr (std::is_same_v<T, float>)
                return glm::mix(a, b, weight);
            else if constexpr (std::is_same_v<T, int32_t>)
                return static_cast<int32_t>(std::round(glm::mix(static_cast<float>(a), static_cast<float>(b), weight)));
            else if constexpr (std::is_same_v<T, Wayfinder::Float3>)
                return glm::mix(a, b, weight);
            else if constexpr (std::is_same_v<T, Wayfinder::Color>)
                return LerpColor(a, b, weight);
            else
                return b;
        }, current);
    }

    void BlendEffectInto(
        Wayfinder::PostProcessEffect& result,
        const Wayfinder::PostProcessEffect& source,
        float weight)
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
                result.Parameters[key] = value;
            }
        }
    }
}

namespace Wayfinder
{
    // ── PostProcessEffect accessors ──────────────────────────

    float PostProcessEffect::GetFloat(const std::string& name, float fallback) const
    {
        auto it = Parameters.find(name);
        if (it == Parameters.end()) return fallback;
        if (const auto* v = std::get_if<float>(&it->second)) return *v;
        if (const auto* v = std::get_if<int32_t>(&it->second)) return static_cast<float>(*v);
        return fallback;
    }

    int32_t PostProcessEffect::GetInt(const std::string& name, int32_t fallback) const
    {
        auto it = Parameters.find(name);
        if (it == Parameters.end()) return fallback;
        if (const auto* v = std::get_if<int32_t>(&it->second)) return *v;
        if (const auto* v = std::get_if<float>(&it->second)) return static_cast<int32_t>(*v);
        return fallback;
    }

    Float3 PostProcessEffect::GetFloat3(const std::string& name, const Float3& fallback) const
    {
        auto it = Parameters.find(name);
        if (it == Parameters.end()) return fallback;
        if (const auto* v = std::get_if<Float3>(&it->second)) return *v;
        return fallback;
    }

    Color PostProcessEffect::GetColor(const std::string& name, const Color& fallback) const
    {
        auto it = Parameters.find(name);
        if (it == Parameters.end()) return fallback;
        if (const auto* v = std::get_if<Color>(&it->second)) return *v;
        return fallback;
    }

    // ── PostProcessStack ─────────────────────────────────────

    const PostProcessEffect* PostProcessStack::FindEffect(const std::string& type) const
    {
        auto it = Effects.find(type);
        return it != Effects.end() ? &it->second : nullptr;
    }

    bool PostProcessStack::HasEffect(const std::string& type) const
    {
        return Effects.contains(type);
    }

    // ── Blending ─────────────────────────────────────────────

    PostProcessStack BlendPostProcessVolumes(
        const Float3& cameraPosition,
        std::span<const PostProcessVolumeInstance> volumes)
    {
        PostProcessStack result;

        if (volumes.empty()) { return result; }

        // Sort by priority (ascending) — higher priority volumes layer on last and dominate.
        std::vector<const PostProcessVolumeInstance*> sorted;
        sorted.reserve(volumes.size());
        for (const auto& instance : volumes)
        {
            if (!instance.Volume) continue;
            sorted.push_back(&instance);
        }

        std::stable_sort(sorted.begin(), sorted.end(), [](const auto* a, const auto* b)
        {
            return a->Volume->Priority < b->Volume->Priority;
        });

        for (const auto* instance : sorted)
        {
             if (!instance->Volume) { continue; }  

            const float distance = ComputeDistanceToVolume(  
                *instance->Volume, instance->WorldPosition, instance->WorldScale,
                instance->LocalToWorld, cameraPosition);
            const float weight = ComputeBlendWeight(distance, instance->Volume->BlendDistance);

            if (weight <= 0.0f) { continue; }

            for (const auto& effect : instance->Volume->Effects)
            {
                if (!effect.Enabled) { continue; }

                auto it = result.Effects.find(effect.Type);
                if (it != result.Effects.end())
                {
                    BlendEffectInto(it->second, effect, weight);
                }
                else
                {
                    // First contribution for this effect type — copy it directly.
                    PostProcessEffect& entry = result.Effects[effect.Type];
                    entry.Type = effect.Type;
                    entry.Enabled = true;
                    entry.Parameters = effect.Parameters;
                }
            }
        }

        return result;
    }
} // namespace Wayfinder
