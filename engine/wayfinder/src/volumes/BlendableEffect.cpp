#include "BlendableEffect.h"

#include "BlendableEffectRegistry.h"

#include "core/Assert.h"
#include "core/Log.h"
#include "maths/Maths.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

namespace Wayfinder
{
    BlendableEffectStack::~BlendableEffectStack()
    {
        // Payloads are constrained to trivially copyable (and therefore trivially destructible)
        // by the BlendableEffectPayload concept, so clearing the vector is sufficient.
        Effects.clear();
    }

    namespace
    {
        float ComputeDistanceToVolume(const VolumeInstance& instance, const Float3& cameraPosition)
        {
            const BlendableEffectVolumeComponent& volume = *instance.Volume;
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

    void BlendableEffect::DestroyPayload(const BlendableEffectRegistry& registry)
    {
        if (TypeId == INVALID_BLENDABLE_EFFECT_ID)
        {
            return;
        }
        const BlendableEffectDesc* desc = registry.Find(TypeId);
        if (desc == nullptr || desc->Destroy == nullptr)
        {
            return;
        }
        // Type-erased payload buffer; registry Destroy runs the correct destructor for TypeId.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        desc->Destroy(Payload);
    }

    const BlendableEffect* BlendableEffectStack::FindEffect(const BlendableEffectId id) const
    {
        const auto it = std::ranges::find_if(Effects, [id](const BlendableEffect& e)
        {
            return e.TypeId == id;
        });
        if (it == Effects.end())
        {
            return nullptr;
        }
        return &*it;
    }

    BlendableEffect* BlendableEffectStack::FindEffectMutable(const BlendableEffectId id)
    {
        const auto it = std::ranges::find_if(Effects, [id](const BlendableEffect& e)
        {
            return e.TypeId == id;
        });
        if (it == Effects.end())
        {
            return nullptr;
        }
        return &*it;
    }

    BlendableEffect& BlendableEffectStack::GetOrCreate(const BlendableEffectId id, const BlendableEffectRegistry& registry)
    {
        if (BlendableEffect* existing = FindEffectMutable(id))
        {
            return *existing;
        }

        const BlendableEffectDesc* desc = registry.Find(id);
        // NOLINTBEGIN(readability-simplify-boolean-expr)
        WAYFINDER_ASSERT(desc != nullptr, "BlendableEffectDesc not found in registry");
        WAYFINDER_ASSERT(desc->CreateIdentity != nullptr, "BlendableEffectDesc has no CreateIdentity function");
        // NOLINTEND(readability-simplify-boolean-expr)

        BlendableEffect effect{};
        effect.TypeId = id;
        effect.Enabled = true;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        desc->CreateIdentity(effect.Payload);
        Effects.push_back(effect);
        return Effects.back();
    }

    void BlendableEffectStack::Clear(const BlendableEffectRegistry& registry)
    {
        for (BlendableEffect& e : Effects)
        {
            e.DestroyPayload(registry);
        }
        Effects.clear();
    }

    BlendableEffectStack BlendVolumeEffects(const Float3& cameraPosition, const std::span<const VolumeInstance> volumes, const BlendableEffectRegistry& registry)
    {
        BlendableEffectStack result;
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
                if (!effect.Enabled || effect.TypeId == INVALID_BLENDABLE_EFFECT_ID)
                {
                    continue;
                }

                const BlendableEffectDesc* desc = registry.Find(effect.TypeId);
                if (desc == nullptr || desc->CreateIdentity == nullptr || desc->Blend == nullptr)
                {
                    continue;
                }

                BlendableEffect& slot = result.GetOrCreate(effect.TypeId, registry);
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

    bool IsValidEffectTypeName(const std::string_view normalisedLower, const BlendableEffectRegistry* registry)
    {
        if (registry != nullptr)
        {
            return registry->FindIdByName(normalisedLower).has_value();
        }
        // No registry — enforce a stable snake_case identifier so scene data stays loadable once a registry exists.
        if (normalisedLower.empty())
        {
            return false;
        }
        const auto first = static_cast<unsigned char>(normalisedLower[0]);
        if (std::islower(first) == 0)
        {
            return false;
        }
        for (std::size_t i = 1; i < normalisedLower.size(); ++i)
        {
            const auto c = static_cast<unsigned char>(normalisedLower[i]);
            if (std::islower(c) != 0 || std::isdigit(c) != 0 || c == '_')
            {
                continue;
            }
            return false;
        }
        return true;
    }

} // namespace Wayfinder
