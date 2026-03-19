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

        // Box: axis-aligned half-extents scaled by entity scale
        const Float3 halfExtents = (volume.Dimensions * 0.5f) * glm::abs(worldScale);
        const Float3 absOffset = glm::abs(offset);
        const Float3 outside = glm::max(absOffset - halfExtents, Float3(0.0f));
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

    void ApplyOverrides(
        Wayfinder::PostProcessSettings& settings,
        const Wayfinder::PostProcessOverrides& overrides,
        float weight)
    {
        if (overrides.Exposure)
            settings.Exposure = glm::mix(settings.Exposure, *overrides.Exposure, weight);
        if (overrides.FogDensity)
            settings.FogDensity = glm::mix(settings.FogDensity, *overrides.FogDensity, weight);
        if (overrides.FogColor)
            settings.FogColor = LerpColor(settings.FogColor, *overrides.FogColor, weight);
        if (overrides.BloomThreshold)
            settings.BloomThreshold = glm::mix(settings.BloomThreshold, *overrides.BloomThreshold, weight);
        if (overrides.BloomIntensity)
            settings.BloomIntensity = glm::mix(settings.BloomIntensity, *overrides.BloomIntensity, weight);
        if (overrides.Vignette)
            settings.Vignette = glm::mix(settings.Vignette, *overrides.Vignette, weight);
    }
}

namespace Wayfinder
{
    PostProcessSettings BlendPostProcessVolumes(
        const Float3& cameraPosition,
        std::span<const PostProcessVolumeInstance> volumes)
    {
        PostProcessSettings result;

        if (volumes.empty()) { return result; }

        // Sort by priority (ascending) — higher priority volumes layer on last and dominate.
        std::vector<const PostProcessVolumeInstance*> sorted;
        sorted.reserve(volumes.size());
        for (const auto& instance : volumes) { sorted.push_back(&instance); }

        std::sort(sorted.begin(), sorted.end(), [](const auto* a, const auto* b)
        {
            return a->Volume->Priority < b->Volume->Priority;
        });

        for (const auto* instance : sorted)
        {
            const float distance = ComputeDistanceToVolume(
                *instance->Volume, instance->WorldPosition, instance->WorldScale, cameraPosition);
            const float weight = ComputeBlendWeight(distance, instance->Volume->BlendDistance);

            if (weight <= 0.0f) { continue; }

            ApplyOverrides(result, instance->Volume->Overrides, weight);
        }

        return result;
    }
} // namespace Wayfinder
