#pragma once

#include "Override.h"
#include "PostProcessRegistry.h"

#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

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

    struct ColourGradingParams;
    struct VignetteParams;
    struct ChromaticAberrationParams;

    // ── ADL hooks (BlendablePostProcessEffect) ─────────────────────────────

    [[nodiscard]] ColourGradingParams Identity(PostProcessTag<ColourGradingParams>);
    [[nodiscard]] ColourGradingParams Lerp(const ColourGradingParams& current, const ColourGradingParams& source, float weight);
    void Serialise(nlohmann::json& json, const ColourGradingParams& params);
    [[nodiscard]] ColourGradingParams Deserialise(PostProcessTag<ColourGradingParams>, const nlohmann::json& json);

    [[nodiscard]] VignetteParams Identity(PostProcessTag<VignetteParams>);
    [[nodiscard]] VignetteParams Lerp(const VignetteParams& current, const VignetteParams& source, float weight);
    void Serialise(nlohmann::json& json, const VignetteParams& params);
    [[nodiscard]] VignetteParams Deserialise(PostProcessTag<VignetteParams>, const nlohmann::json& json);

    [[nodiscard]] ChromaticAberrationParams Identity(PostProcessTag<ChromaticAberrationParams>);
    [[nodiscard]] ChromaticAberrationParams Lerp(const ChromaticAberrationParams& current, const ChromaticAberrationParams& source, float weight);
    void Serialise(nlohmann::json& json, const ChromaticAberrationParams& params);
    [[nodiscard]] ChromaticAberrationParams Deserialise(PostProcessTag<ChromaticAberrationParams>, const nlohmann::json& json);

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
    };

    struct VignetteParams
    {
        Override<float> Strength{0.0f};
    };

    struct ChromaticAberrationParams
    {
        Override<float> Intensity{0.0f};
    };

    /**
     * @brief One effect instance stored on a volume or in the blended stack (type-erased payload).
     */
    struct WAYFINDER_API PostProcessEffect
    {
        PostProcessEffectId TypeId = INVALID_POST_PROCESS_EFFECT_ID;
        bool Enabled = true;
        alignas(16) std::byte Payload[POST_PROCESS_EFFECT_PAYLOAD_CAPACITY]{};

        void DestroyPayload(const PostProcessRegistry& registry);
    };

    /**
     * @brief Blended result: at most one entry per PostProcessEffectId.
     */
    struct WAYFINDER_API PostProcessStack
    {
        std::vector<PostProcessEffect> Effects;

        ~PostProcessStack();

        [[nodiscard]] const PostProcessEffect* FindEffect(PostProcessEffectId id) const;

        [[nodiscard]] PostProcessEffect* FindEffectMutable(PostProcessEffectId id);

        /**
         * @brief Returns existing slot or creates one with identity payload (CreateIdentity from registry).
         */
        [[nodiscard]] PostProcessEffect& GetOrCreate(PostProcessEffectId id, const PostProcessRegistry& registry);

        void Clear(const PostProcessRegistry& registry);

        template<typename T>
        [[nodiscard]] const T* FindPayload(PostProcessEffectId id) const
        {
            const PostProcessEffect* e = FindEffect(id);
            if (e == nullptr || !e->Enabled)
            {
                return nullptr;
            }
            return std::launder(reinterpret_cast<const T*>(e->Payload));
        }
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

    WAYFINDER_API PostProcessStack BlendPostProcessVolumes(const Float3& cameraPosition, std::span<const PostProcessVolumeInstance> volumes, const PostProcessRegistry& registry);

    [[nodiscard]] WAYFINDER_API ColourGradingParams ResolveColourGradingForView(const PostProcessStack& stack, PostProcessEffectId id);
    [[nodiscard]] WAYFINDER_API VignetteParams ResolveVignetteForView(const PostProcessStack& stack, PostProcessEffectId id);
    [[nodiscard]] WAYFINDER_API ChromaticAberrationParams ResolveChromaticAberrationForView(const PostProcessStack& stack, PostProcessEffectId id);

    /**
     * @brief Normalise effect type string for lookup (lowercase ASCII).
     */
    [[nodiscard]] WAYFINDER_API std::string NormalisePostProcessEffectTypeString(std::string_view name);

    /**
     * @brief True if the name matches a registered type (uses validation instance when set).
     */
    [[nodiscard]] WAYFINDER_API bool IsValidPostProcessEffectTypeName(std::string_view normalisedLower);

} // namespace Wayfinder
