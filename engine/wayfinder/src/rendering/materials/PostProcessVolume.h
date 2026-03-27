#pragma once

#include <map>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "core/Types.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    /** @brief Shape of a post-process influence volume. */
    enum class PostProcessVolumeShape
    {
        Global,
        Box,
        Sphere
    };

    /** @brief Authoring / runtime effect kind — resolved from JSON `type` strings at load time. */
    enum class PostProcessEffectType : uint8_t
    {
        Unknown = 0,
        ColourGrading,
        Bloom,
    };

    /** @brief Parameters for final-screen colour grading (lift / gamma / gain, exposure, contrast). */
    struct ColourGradingParams
    {
        float ExposureStops = 0.0f;
        float Contrast = 1.0f;
        float Saturation = 1.0f;
        Float3 Lift = {0.0f, 0.0f, 0.0f};
        Float3 Gamma = {1.0f, 1.0f, 1.0f};
        Float3 Gain = {1.0f, 1.0f, 1.0f};
        float VignetteStrength = 0.0f;
        float ChromaticAberrationIntensity = 0.0f;
    };

    /** @brief Bloom — reserved for dedicated bloom passes (parameters only; graph passes consume these later). */
    struct BloomParams
    {
        float Threshold = 1.0f;
        float Intensity = 0.0f;
        float Radius = 1.0f;
    };

    /** @brief Typed payload for a single effect instance. */
    using PostProcessEffectPayload = std::variant<std::monostate, ColourGradingParams, BloomParams>;

    /**
     * @struct PostProcessEffect
     * @brief One effect with a typed payload (no stringly-typed parameter maps at runtime).
     */
    struct WAYFINDER_API PostProcessEffect
    {
        PostProcessEffectType Type = PostProcessEffectType::Unknown;
        bool Enabled = true;
        PostProcessEffectPayload Payload{};

        /** @brief Parse authoring string (e.g. JSON `type`) to enum; returns Unknown if unrecognised. */
        static PostProcessEffectType ParseTypeString(std::string_view name);

        /** @brief Serialise effect type for JSON / tools. */
        static std::string_view TypeToString(PostProcessEffectType type);
    };

    /**
     * @struct PostProcessVolumeComponent
     * @brief ECS component placed on scene entities to define post-processing influence volumes.
     */
    struct PostProcessVolumeComponent
    {
        PostProcessVolumeShape Shape = PostProcessVolumeShape::Global;
        int Priority = 0;
        float BlendDistance = 0.0f;
        Float3 Dimensions = {10.0f, 10.0f, 10.0f};
        float Radius = 10.0f;
        std::vector<PostProcessEffect> Effects;

        PostProcessVolumeComponent() = default;
        PostProcessVolumeComponent(const PostProcessVolumeComponent&) = default;
        PostProcessVolumeComponent& operator=(const PostProcessVolumeComponent&) = default;
    };

    /**
     * @struct PostProcessVolumeInstance
     * @brief Input to the blending function: a volume paired with its world-space transform.
     */
    struct PostProcessVolumeInstance
    {
        const PostProcessVolumeComponent* Volume = nullptr;
        Float3 WorldPosition = {0.0f, 0.0f, 0.0f};
        Float3 WorldScale = {1.0f, 1.0f, 1.0f};
        Matrix4 LocalToWorld = Matrix4(1.0f);
    };

    /**
     * @struct PostProcessStack
     * @brief Blended result keyed by effect type.
     */
    struct WAYFINDER_API PostProcessStack
    {
        std::map<PostProcessEffectType, PostProcessEffect> Effects;

        const PostProcessEffect* FindEffect(PostProcessEffectType type) const;
        bool HasEffect(PostProcessEffectType type) const;
    };

    /**
     * @brief Evaluate all active volumes against the camera and produce a blended stack.
     */
    WAYFINDER_API PostProcessStack BlendPostProcessVolumes(const Float3& cameraPosition, std::span<const PostProcessVolumeInstance> volumes);

    /** @brief Merge blended colour grading contributions into one struct for GPU upload. */
    WAYFINDER_API ColourGradingParams ResolveColourGradingForView(const PostProcessStack& stack);
} // namespace Wayfinder
