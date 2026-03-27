#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
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

    /** @brief Value type for post-process effect parameters. */
    using PostProcessParamValue = std::variant<float, int32_t, Float3, Colour>;

    /**
     * @struct PostProcessEffect
     * @brief A named post-processing effect with a generic parameter bag.
     *
     * Effects are stacked inside PostProcessVolumeComponents and blended across
     * volumes. Render passes consume these by querying their effect type from
     * the blended stack.
     */
    struct WAYFINDER_API PostProcessEffect
    {
        std::string Type;                                                  ///< Effect identifier, e.g. "exposure", "bloom", "fog", "vignette".
        bool Enabled = true;                                               ///< Whether this effect contributes to the blended stack.
        std::unordered_map<std::string, PostProcessParamValue> Parameters; ///< Named parameter bag.

        /** @brief Retrieve a float parameter, returning @p fallback if not found. */
        float GetFloat(std::string_view name, float fallback = 0.0f) const;
        /** @brief Retrieve an integer parameter, returning @p fallback if not found. */
        int32_t GetInt(std::string_view name, int32_t fallback = 0) const;
        /** @brief Retrieve a Float3 parameter, returning @p fallback if not found. */
        Float3 GetFloat3(std::string_view name, const Float3& fallback = {0.0f, 0.0f, 0.0f}) const;
        /** @brief Retrieve a Colour parameter, returning @p fallback if not found. */
        Colour GetColour(std::string_view name, const Colour& fallback = Colour::White()) const;

        /** @brief Set a float parameter. */
        void SetFloat(std::string_view name, float v)
        {
            Parameters[std::string(name)] = v;
        }
        /** @brief Set an integer parameter. */
        void SetInt(std::string_view name, int32_t v)
        {
            Parameters[std::string(name)] = v;
        }
        /** @brief Set a Float3 parameter. */
        void SetFloat3(std::string_view name, const Float3& v)
        {
            Parameters[std::string(name)] = v;
        }
        /** @brief Set a Colour parameter. */
        void SetColour(std::string_view name, const Colour& v)
        {
            Parameters[std::string(name)] = v;
        }
    };

    /**
     * @struct PostProcessVolumeComponent
     * @brief ECS component placed on scene entities to define post-processing influence volumes.
     */
    struct PostProcessVolumeComponent
    {
        PostProcessVolumeShape Shape = PostProcessVolumeShape::Global; ///< Volume shape (Global applies everywhere).
        int Priority = 0;                                              ///< Blend ordering — higher priority volumes layer on last.
        float BlendDistance = 0.0f;                                    ///< Distance over which the volume fades in (0 = hard cut).
        Float3 Dimensions = {10.0f, 10.0f, 10.0f};                     ///< Full extents for Box shape.
        float Radius = 10.0f;                                          ///< Radius for Sphere shape.
        std::vector<PostProcessEffect> Effects;                        ///< Effects contributed by this volume.

        PostProcessVolumeComponent() = default;
        PostProcessVolumeComponent(const PostProcessVolumeComponent&) = default;
        PostProcessVolumeComponent& operator=(const PostProcessVolumeComponent&) = default;
    };

    /**
     * @struct PostProcessVolumeInstance
     * @brief Input to the blending function: a volume paired with its world-space transform.
     *
     * Volume may be nullptr — BlendPostProcessVolumes() skips null entries,
     * so callers can safely push default-constructed instances.
     */
    struct PostProcessVolumeInstance
    {
        const PostProcessVolumeComponent* Volume = nullptr; ///< The volume data (may be nullptr).
        Float3 WorldPosition = {0.0f, 0.0f, 0.0f};          ///< World-space position of the volume entity.
        Float3 WorldScale = {1.0f, 1.0f, 1.0f};             ///< World-space scale of the volume entity.
        Matrix4 LocalToWorld = Matrix4(1.0f);               ///< Full local-to-world transform matrix.
    };

    /**
     * @struct PostProcessStack
     * @brief Blended result: per-effect-type parameter blocks, ready for consumption by render passes.
     */
    struct WAYFINDER_API PostProcessStack
    {
        std::unordered_map<std::string, PostProcessEffect> Effects; ///< Blended effects keyed by type.

        /** @brief Find a blended effect by type, or nullptr if absent. */
        const PostProcessEffect* FindEffect(std::string_view type) const;
        /** @brief Returns true if the stack contains the given effect type. */
        bool HasEffect(std::string_view type) const;
    };

    /**
     * @brief Evaluate all active volumes against the camera position and produce a blended stack.
     * @param cameraPosition World-space position of the camera used for distance-based blending.
     * @param volumes        Span of volume instances to evaluate (null Volume entries are skipped).
     * @return A PostProcessStack with all contributing effects blended by priority and distance.
     */
    WAYFINDER_API PostProcessStack BlendPostProcessVolumes(const Float3& cameraPosition, std::span<const PostProcessVolumeInstance> volumes);
} // namespace Wayfinder
