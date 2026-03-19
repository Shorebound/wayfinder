#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

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

    // Value type for post-process effect parameters.
    using PostProcessParamValue = std::variant<float, int32_t, Float3, Color>;

    // A named post-processing effect with a generic parameter bag.
    // Effects are stacked inside PostProcessVolumeComponents and blended across volumes.
    // RenderFeatures consume these by querying their effect type from the blended stack.
    struct WAYFINDER_API PostProcessEffect
    {
        std::string Type; // e.g. "exposure", "bloom", "fog", "vignette"
        bool Enabled = true;
        std::unordered_map<std::string, PostProcessParamValue> Parameters;

        float GetFloat(const std::string& name, float fallback = 0.0f) const;
        int32_t GetInt(const std::string& name, int32_t fallback = 0) const;
        Float3 GetFloat3(const std::string& name, const Float3& fallback = {0.0f, 0.0f, 0.0f}) const;
        Color GetColor(const std::string& name, const Color& fallback = Color::White()) const;

        void SetFloat(const std::string& name, float v) { Parameters[name] = v; }
        void SetInt(const std::string& name, int32_t v) { Parameters[name] = v; }
        void SetFloat3(const std::string& name, const Float3& v) { Parameters[name] = v; }
        void SetColor(const std::string& name, const Color& v) { Parameters[name] = v; }
    };

    // ECS component placed on scene entities to define post-processing influence volumes.
    struct PostProcessVolumeComponent
    {
        PostProcessVolumeShape Shape = PostProcessVolumeShape::Global;
        int Priority = 0;
        float BlendDistance = 0.0f;
        Float3 Dimensions = {10.0f, 10.0f, 10.0f}; // full extents for Box shape
        float Radius = 10.0f;                        // radius for Sphere shape
        std::vector<PostProcessEffect> Effects;

        PostProcessVolumeComponent() = default;
        PostProcessVolumeComponent(const PostProcessVolumeComponent&) = default;
    };

    // Input to the blending function: a volume paired with its world-space transform.
    // Volume may be nullptr — BlendPostProcessVolumes() skips null entries.
    struct PostProcessVolumeInstance
    {
        const PostProcessVolumeComponent* Volume = nullptr;
        Float3 WorldPosition = {0.0f, 0.0f, 0.0f};
        Float3 WorldScale = {1.0f, 1.0f, 1.0f};
        Matrix4 LocalToWorld = Matrix4(1.0f);
    };

    // Blended result: per-effect-type parameter blocks, ready for consumption by RenderFeatures.
    struct WAYFINDER_API PostProcessStack
    {
        std::unordered_map<std::string, PostProcessEffect> Effects;

        const PostProcessEffect* FindEffect(const std::string& type) const;
        bool HasEffect(const std::string& type) const;
    };

    // Evaluate all active volumes against the camera position and produce a blended stack.
    WAYFINDER_API PostProcessStack BlendPostProcessVolumes(
        const Float3& cameraPosition,
        std::span<const PostProcessVolumeInstance> volumes);
} // namespace Wayfinder
