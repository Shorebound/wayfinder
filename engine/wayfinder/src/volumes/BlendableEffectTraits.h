#pragma once

#include "volumes/OverrideReflection.h"

#include <nlohmann/json_fwd.hpp>

namespace Wayfinder
{
    /**
     * @brief Traits interface for blendable effect types.
     *
     * Specialise explicitly for custom behaviour (e.g. alt-key deserialisation), or
     * provide a `static constexpr auto FIELDS` tuple of FieldDesc entries for automatic
     * Identity / Lerp / Serialise / Deserialise support via the default specialisation.
     */
    template<typename T>
    struct BlendableEffectTraits;

    /**
     * @brief Default specialisation: derives all operations from T::FIELDS.
     */
    template<typename T>
        requires requires { T::FIELDS; }
    struct BlendableEffectTraits<T>
    {
        static T Identity()
        {
            return T{};
        }

        static T Lerp(const T& current, const T& source, float weight)
        {
            T out{};
            LerpFields(out, current, source, weight, T::FIELDS);
            return out;
        }

        static void Serialise(nlohmann::json& json, const T& params)
        {
            SerialiseFields(json, params, T::FIELDS);
        }

        static T Deserialise(const nlohmann::json& json)
        {
            T p{};
            DeserialiseFields(json, p, T::FIELDS);
            return p;
        }
    };

} // namespace Wayfinder
