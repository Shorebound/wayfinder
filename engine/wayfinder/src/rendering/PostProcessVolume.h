#pragma once

#include <optional>
#include <span>

#include "RenderTypes.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    enum class PostProcessVolumeShape
    {
        Global,
        Box,
        Sphere
    };

    // Blended post-processing parameters — concrete values consumed by render features.
    struct PostProcessSettings
    {
        float Exposure = 1.0f;
        float FogDensity = 0.0f;
        Color FogColor = Color{.r = 180, .g = 200, .b = 220, .a = 255};
        float BloomThreshold = 1.5f;
        float BloomIntensity = 0.0f;
        float Vignette = 0.0f;
    };

    // Per-volume partial overrides — only set fields participate in blending.
    struct PostProcessOverrides
    {
        std::optional<float> Exposure;
        std::optional<float> FogDensity;
        std::optional<Color> FogColor;
        std::optional<float> BloomThreshold;
        std::optional<float> BloomIntensity;
        std::optional<float> Vignette;
    };

    // ECS component placed on scene entities to define post-processing influence volumes.
    struct PostProcessVolumeComponent
    {
        PostProcessVolumeShape Shape = PostProcessVolumeShape::Global;
        int Priority = 0;
        float BlendDistance = 0.0f;
        Float3 Dimensions = {10.0f, 10.0f, 10.0f}; // full extents for Box shape
        float Radius = 10.0f;                        // radius for Sphere shape
        PostProcessOverrides Overrides;

        PostProcessVolumeComponent() = default;
        PostProcessVolumeComponent(const PostProcessVolumeComponent&) = default;
    };

    // Input to the blending function: a volume paired with its world-space transform.
    struct PostProcessVolumeInstance
    {
        const PostProcessVolumeComponent* Volume = nullptr;
        Float3 WorldPosition = {0.0f, 0.0f, 0.0f};
        Float3 WorldScale = {1.0f, 1.0f, 1.0f};
    };

    // Evaluate all active volumes against the camera position and produce blended settings.
    WAYFINDER_API PostProcessSettings BlendPostProcessVolumes(
        const Float3& cameraPosition,
        std::span<const PostProcessVolumeInstance> volumes);
} // namespace Wayfinder
