#pragma once

#include "BlendableEffectRegistry.h"
#include "Override.h"

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
    /** @brief Shape of an influence volume. */
    enum class VolumeShape
    {
        Global,
        Box,
        Sphere
    };

    /**
     * @brief One effect instance stored on a volume or in the blended stack (type-erased payload).
     */
#ifdef WAYFINDER_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4324) // alignas(16) payload forces tail padding; layout is intentional
#endif
    struct WAYFINDER_API BlendableEffect
    {
        alignas(16) std::byte Payload[BLENDABLE_EFFECT_PAYLOAD_CAPACITY]{};
        BlendableEffectId TypeId = INVALID_BLENDABLE_EFFECT_ID;
        bool Enabled = true;

        void DestroyPayload(const BlendableEffectRegistry& registry);
    };
#ifdef WAYFINDER_COMPILER_MSVC
#pragma warning(pop)
#endif

    /**
     * @brief Blended result: at most one entry per BlendableEffectId.
     */
    struct WAYFINDER_API BlendableEffectStack
    {
        std::vector<BlendableEffect> Effects;

        ~BlendableEffectStack();

        [[nodiscard]] const BlendableEffect* FindEffect(BlendableEffectId id) const;

        [[nodiscard]] BlendableEffect* FindEffectMutable(BlendableEffectId id);

        /**
         * @brief Returns existing slot or creates one with identity payload (CreateIdentity from registry).
         */
        [[nodiscard]] BlendableEffect& GetOrCreate(BlendableEffectId id, const BlendableEffectRegistry& registry);

        void Clear(const BlendableEffectRegistry& registry);

        template<typename T>
            requires BlendableEffectPayload<T>
        [[nodiscard]] const T* FindPayload(BlendableEffectId id) const
        {
            const BlendableEffect* e = FindEffect(id);
            if (e == nullptr || !e->Enabled)
            {
                return nullptr;
            }
            return std::launder(reinterpret_cast<const T*>(e->Payload));
        }
    };

    /**
     * @struct BlendableEffectVolumeComponent
     * @brief ECS component placed on scene entities to define influence volumes for blendable effects.
     */
    struct BlendableEffectVolumeComponent
    {
        VolumeShape Shape = VolumeShape::Global;
        int Priority = 0;
        float BlendDistance = 0.0f;
        Float3 Dimensions = {10.0f, 10.0f, 10.0f};
        float Radius = 10.0f;
        std::vector<BlendableEffect> Effects;

        BlendableEffectVolumeComponent() = default;
        BlendableEffectVolumeComponent(const BlendableEffectVolumeComponent&) = default;
        BlendableEffectVolumeComponent& operator=(const BlendableEffectVolumeComponent&) = default;
        BlendableEffectVolumeComponent(BlendableEffectVolumeComponent&&) noexcept = default;
        BlendableEffectVolumeComponent& operator=(BlendableEffectVolumeComponent&&) noexcept = default;
    };

    /**
     * @struct VolumeInstance
     * @brief Input to the blending function: a volume paired with its world-space transform.
     */
    struct VolumeInstance
    {
        const BlendableEffectVolumeComponent* Volume = nullptr;
        Float3 WorldPosition = {0.0f, 0.0f, 0.0f};
        Float3 WorldScale = {1.0f, 1.0f, 1.0f};
        Matrix4 LocalToWorld = Matrix4(1.0f);
    };

    WAYFINDER_API BlendableEffectStack BlendVolumeEffects(const Float3& cameraPosition, std::span<const VolumeInstance> volumes, const BlendableEffectRegistry& registry);

    /**
     * @brief Normalise effect type string for lookup (lowercase ASCII).
     */
    [[nodiscard]] WAYFINDER_API std::string NormaliseEffectTypeString(std::string_view name);

    /**
     * @brief True if the name matches a registered type (uses validation instance when set).
     */
    [[nodiscard]] WAYFINDER_API bool IsValidEffectTypeName(std::string_view normalisedLower, const BlendableEffectRegistry* registry = nullptr);

} // namespace Wayfinder
