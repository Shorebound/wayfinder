#pragma once

#include <new>
#include <utility>

#include <nlohmann/json.hpp>

namespace Wayfinder
{
    template<BlendablePostProcessEffect T>
    PostProcessEffectId PostProcessRegistry::Register(const std::string_view name)
    {
        static_assert(sizeof(T) <= POST_PROCESS_EFFECT_PAYLOAD_CAPACITY, "Register<T>: effect type exceeds POST_PROCESS_EFFECT_PAYLOAD_CAPACITY");
        static_assert(alignof(T) <= 16, "Register<T>: increase PostProcessEffect payload alignment if needed");

        if (m_sealed)
        {
            return INVALID_POST_PROCESS_EFFECT_ID;
        }

        PostProcessEffectDesc desc{};
        m_names.emplace_back(name);
        desc.Name = std::string_view{m_names.back()};
        desc.Size = sizeof(T);
        desc.Align = alignof(T);

        desc.CreateIdentity = [](void* dst)
        {
            std::construct_at(static_cast<T*>(dst), Identity(PostProcessTag<T>{}));
        };

        desc.Destroy = [](void* dst)
        {
            std::destroy_at(static_cast<T*>(dst));
        };

        desc.Blend = [](void* dst, const void* src, const float weight)
        {
            auto& d = *static_cast<T*>(dst);
            const auto& s = *static_cast<const T*>(src);
            d = Lerp(d, s, weight);
        };

        desc.Deserialise = [](void* dst, const nlohmann::json& json)
        {
            std::construct_at(static_cast<T*>(dst), Deserialise(PostProcessTag<T>{}, json));
        };

        desc.Serialise = [](nlohmann::json& json, const void* src)
        {
            Serialise(json, *static_cast<const T*>(src));
        };

        const PostProcessEffectId id = static_cast<PostProcessEffectId>(m_descs.size());
        m_descs.push_back(std::move(desc));
        return id;
    }

} // namespace Wayfinder
