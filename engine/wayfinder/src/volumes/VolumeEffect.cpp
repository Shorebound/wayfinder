#include "VolumeEffect.h"

#include "VolumeEffectRegistry.h"

#include "core/Assert.h"
#include "maths/Maths.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

namespace Wayfinder
{
    VolumeEffectStack::~VolumeEffectStack()
    {
        if (const VolumeEffectRegistry* registry = VolumeEffectRegistry::GetActiveInstance())
        {
            Clear(*registry);
        }
    }

    namespace
    {
        float ComputeDistanceToVolume(const VolumeInstance& instance, const Float3& cameraPosition)
        {
            const VolumeComponent& volume = *instance.Volume;
            if (volume.Shape == VolumeShape::Global)
            {
                return 0.0f;
            }

            const Float3 offset = cameraPosition - instance.WorldPosition;

            if (volume.Shape == VolumeShape::Sphere)
            {
                const Float3 absScale = Maths::Abs(instance.WorldScale);
                const float scaledRadius = volume.Radius * Maths::Max(absScale.x, Maths::Max(absScale.y, absScale.z)); // NOLINT(cppcoreguidelines-pro-type-union-access)
                return Maths::Max(Maths::Length(offset) - scaledRadius, 0.0f);
            }

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

    } // namespace

    void VolumeEffect::DestroyPayload(const VolumeEffectRegistry& registry)
    {
        if (TypeId == INVALID_VOLUME_EFFECT_ID)
        {
            return;
        }
        const VolumeEffectDesc* desc = registry.Find(TypeId);
        if (desc == nullptr || desc->Destroy == nullptr)
        {
            return;
        }
        // Type-erased payload buffer; registry Destroy runs the correct destructor for TypeId.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        desc->Destroy(Payload);
    }

    const VolumeEffect* VolumeEffectStack::FindEffect(const VolumeEffectId id) const
    {
        const auto it = std::ranges::find_if(Effects, [id](const VolumeEffect& e)
        {
            return e.TypeId == id;
        });
        if (it == Effects.end())
        {
            return nullptr;
        }
        return &*it;
    }

    VolumeEffect* VolumeEffectStack::FindEffectMutable(const VolumeEffectId id)
    {
        const auto it = std::ranges::find_if(Effects, [id](const VolumeEffect& e)
        {
            return e.TypeId == id;
        });
        if (it == Effects.end())
        {
            return nullptr;
        }
        return &*it;
    }

    VolumeEffect& VolumeEffectStack::GetOrCreate(const VolumeEffectId id, const VolumeEffectRegistry& registry)
    {
        if (VolumeEffect* existing = FindEffectMutable(id))
        {
            return *existing;
        }

        const VolumeEffectDesc* desc = registry.Find(id);
        // NOLINTBEGIN(readability-simplify-boolean-expr)
        WAYFINDER_ASSERT(desc != nullptr);
        WAYFINDER_ASSERT(desc->CreateIdentity != nullptr);
        // NOLINTEND(readability-simplify-boolean-expr)

        VolumeEffect effect{};
        effect.TypeId = id;
        effect.Enabled = true;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        desc->CreateIdentity(effect.Payload);
        Effects.push_back(effect);
        return Effects.back();
    }

    void VolumeEffectStack::Clear(const VolumeEffectRegistry& registry)
    {
        for (VolumeEffect& e : Effects)
        {
            e.DestroyPayload(registry);
        }
        Effects.clear();
    }

    VolumeEffectStack BlendVolumeEffects(const Float3& cameraPosition, const std::span<const VolumeInstance> volumes, const VolumeEffectRegistry& registry)
    {
        VolumeEffectStack result;
        if (volumes.empty())
        {
            return result;
        }

        std::vector<const VolumeInstance*> sorted;
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
                if (!effect.Enabled || effect.TypeId == INVALID_VOLUME_EFFECT_ID)
                {
                    continue;
                }

                const VolumeEffectDesc* desc = registry.Find(effect.TypeId);
                if (desc == nullptr || desc->Blend == nullptr)
                {
                    continue;
                }

                VolumeEffect& slot = result.GetOrCreate(effect.TypeId, registry);
                // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
                desc->Blend(slot.Payload, effect.Payload, weight);
                // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
            }
        }

        return result;
    }

    std::string NormaliseEffectTypeString(const std::string_view name)
    {
        std::string lower;
        lower.reserve(name.size());
        for (const char c : name)
        {
            lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        return lower;
    }

    bool IsValidEffectTypeName(const std::string_view normalisedLower)
    {
        if (const VolumeEffectRegistry* reg = VolumeEffectRegistry::GetActiveInstance())
        {
            return reg->FindIdByName(normalisedLower).has_value();
        }
        return normalisedLower == "colour_grading" || normalisedLower == "vignette" || normalisedLower == "chromatic_aberration";
    }

} // namespace Wayfinder
