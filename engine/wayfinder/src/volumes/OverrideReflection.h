#pragma once

#include "volumes/Override.h"

#include "core/Types.h"

#include <string_view>
#include <tuple>

#include <nlohmann/json_fwd.hpp>

namespace Wayfinder
{
    /** @brief Blend policy: linear interpolation via LerpOverride. */
    struct LinearBlend {};

    /**
     * @brief Compile-time field descriptor binding a pointer-to-member to a JSON key and blend policy.
     * @tparam TStruct The owning struct type.
     * @tparam TValue The underlying value type (e.g. float, Float3).
     * @tparam TPolicy Blend policy tag (default: LinearBlend).
     */
    template<typename TStruct, typename TValue, typename TPolicy = LinearBlend>
    struct FieldDesc
    {
        Override<TValue> TStruct::* Member;
        std::string_view Key;
    };

    /// CTAD: deduce policy as LinearBlend when omitted.
    template<typename TStruct, typename TValue>
    FieldDesc(Override<TValue> TStruct::*, std::string_view) -> FieldDesc<TStruct, TValue, LinearBlend>;

    // ── Single-field blend ──────────────────────────────────────────────────

    /** @brief Blend a single LinearBlend field via LerpOverride. */
    template<typename TStruct, typename TValue>
    void BlendField(TStruct& out, const TStruct& current, const TStruct& source, float weight, const FieldDesc<TStruct, TValue, LinearBlend>& field)
    {
        out.*field.Member = LerpOverride(current.*field.Member, source.*field.Member, weight);
    }

    // ── Per-type serialisation helpers (defined in .cpp) ────────────────────

    void WriteOverrideField(nlohmann::json& json, std::string_view key, const Override<float>& field);
    void WriteOverrideField(nlohmann::json& json, std::string_view key, const Override<Float3>& field);
    void ReadOverrideField(const nlohmann::json& json, std::string_view key, Override<float>& field);
    void ReadOverrideField(const nlohmann::json& json, std::string_view key, Override<Float3>& field);

    // ── Fold expressions over FIELDS tuple ──────────────────────────────────

    /** @brief Blend all fields described by a FIELDS tuple. */
    template<typename TStruct, typename... TDescs>
    void LerpFields(TStruct& out, const TStruct& current, const TStruct& source, float weight, const std::tuple<TDescs...>& descs)
    {
        std::apply([&](const auto&... field)
        {
            (BlendField(out, current, source, weight, field), ...);
        }, descs);
    }

    /** @brief Write all active Override fields to JSON. */
    template<typename TStruct, typename... TDescs>
    void SerialiseFields(nlohmann::json& json, const TStruct& params, const std::tuple<TDescs...>& descs)
    {
        std::apply([&](const auto&... field)
        {
            (WriteOverrideField(json, field.Key, params.*field.Member), ...);
        }, descs);
    }

    /** @brief Read Override fields from JSON. */
    template<typename TStruct, typename... TDescs>
    void DeserialiseFields(const nlohmann::json& json, TStruct& params, const std::tuple<TDescs...>& descs)
    {
        std::apply([&](const auto&... field)
        {
            (ReadOverrideField(json, field.Key, params.*field.Member), ...);
        }, descs);
    }

} // namespace Wayfinder
