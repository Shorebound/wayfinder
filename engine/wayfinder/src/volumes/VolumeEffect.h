#pragma once

#include "Override.h"
#include "VolumeEffectRegistry.h"

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
    struct WAYFINDER_API VolumeEffect
    {
        VolumeEffectId TypeId = INVALID_VOLUME_EFFECT_ID;
        bool Enabled = true;
        alignas(16) std::byte Payload[VOLUME_EFFECT_PAYLOAD_CAPACITY]{};

        void DestroyPayload(const VolumeEffectRegistry& registry);
    };

    /**
     * @brief Blended result: at most one entry per VolumeEffectId.
     */
    struct WAYFINDER_API VolumeEffectStack
    {
        std::vector<VolumeEffect> Effects;

        ~VolumeEffectStack();

        [[nodiscard]] const VolumeEffect* FindEffect(VolumeEffectId id) const;

        [[nodiscard]] VolumeEffect* FindEffectMutable(VolumeEffectId id);

        /**
         * @brief Returns existing slot or creates one with identity payload (CreateIdentity from registry).
         */
        [[nodiscard]] VolumeEffect& GetOrCreate(VolumeEffectId id, const VolumeEffectRegistry& registry);

        void Clear(const VolumeEffectRegistry& registry);

        template<typename T>
        [[nodiscard]] const T* FindPayload(VolumeEffectId id) const
        {
            const VolumeEffect* e = FindEffect(id);
            if (e == nullptr || !e->Enabled)
            {
                return nullptr;
            }
            return std::launder(reinterpret_cast<const T*>(e->Payload));
        }
    };

    /**
     * @struct VolumeComponent
     * @brief ECS component placed on scene entities to define influence volumes.
     */
    struct VolumeComponent
    {
        VolumeShape Shape = VolumeShape::Global;
        int Priority = 0;
        float BlendDistance = 0.0f;
        Float3 Dimensions = {10.0f, 10.0f, 10.0f};
        float Radius = 10.0f;
        std::vector<VolumeEffect> Effects;

        VolumeComponent() = default;
        VolumeComponent(const VolumeComponent&) = default;
        VolumeComponent& operator=(const VolumeComponent&) = default;
    };

    /**
     * @struct VolumeInstance
     * @brief Input to the blending function: a volume paired with its world-space transform.
     */
    struct VolumeInstance
    {
        const VolumeComponent* Volume = nullptr;
        Float3 WorldPosition = {0.0f, 0.0f, 0.0f};
        Float3 WorldScale = {1.0f, 1.0f, 1.0f};
        Matrix4 LocalToWorld = Matrix4(1.0f);
    };

    WAYFINDER_API VolumeEffectStack BlendVolumeEffects(const Float3& cameraPosition, std::span<const VolumeInstance> volumes, const VolumeEffectRegistry& registry);

    /**
     * @brief Normalise effect type string for lookup (lowercase ASCII).
     */
    [[nodiscard]] WAYFINDER_API std::string NormaliseEffectTypeString(std::string_view name);

    /**
     * @brief True if the name matches a registered type (uses validation instance when set).
     */
    [[nodiscard]] WAYFINDER_API bool IsValidEffectTypeName(std::string_view normalisedLower);

} // namespace Wayfinder
