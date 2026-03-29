#pragma once

#include <new>
#include <utility>

#include <nlohmann/json.hpp>

namespace Wayfinder
{
    template<BlendableEffectPayload T>
    BlendableEffectId BlendableEffectRegistry::Register(const std::string_view name)
    {
        static_assert(sizeof(T) <= BLENDABLE_EFFECT_PAYLOAD_CAPACITY, "Register<T>: effect type exceeds BLENDABLE_EFFECT_PAYLOAD_CAPACITY");
        static_assert(alignof(T) <= 16, "Register<T>: increase BlendableEffect payload alignment if needed");

        if (m_sealed)
        {
            return INVALID_BLENDABLE_EFFECT_ID;
        }

        BlendableEffectDesc desc{};
        desc.Name = std::string{name};
        desc.Size = sizeof(T);
        desc.Align = alignof(T);

        desc.CreateIdentity = [](void* dst)
        {
            std::construct_at(static_cast<T*>(dst), BlendableEffectTraits<T>::Identity());
        };

        desc.Destroy = [](void* dst)
        {
            std::destroy_at(static_cast<T*>(dst));
        };

        desc.Blend = [](void* dst, const void* src, const float weight)
        {
            auto& d = *static_cast<T*>(dst);
            const auto& s = *static_cast<const T*>(src);
            d = BlendableEffectTraits<T>::Lerp(d, s, weight);
        };

        desc.Deserialise = [](void* dst, const nlohmann::json& json)
        {
            *static_cast<T*>(dst) = BlendableEffectTraits<T>::Deserialise(json);
        };

        desc.Serialise = [](nlohmann::json& json, const void* src)
        {
            BlendableEffectTraits<T>::Serialise(json, *static_cast<const T*>(src));
        };

        const BlendableEffectId id = static_cast<BlendableEffectId>(m_descs.size());
        m_descs.push_back(std::move(desc));
        return id;
    }

} // namespace Wayfinder
