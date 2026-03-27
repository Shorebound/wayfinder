#pragma once

#include "wayfinder_exports.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace Wayfinder
{
    /// Maximum size for type-erased effect payload in BlendableEffect (SBO).
    inline constexpr std::size_t BLENDABLE_EFFECT_PAYLOAD_CAPACITY = 96;

    /** @brief Opaque handle returned by BlendableEffectRegistry::Register. */
    using BlendableEffectId = uint32_t;

    inline constexpr BlendableEffectId INVALID_BLENDABLE_EFFECT_ID = static_cast<BlendableEffectId>(-1);

    /**
     * @brief Canonical type names for engine-registered blendable effects (single source of truth for registration and validation).
     */
    namespace EngineBlendableEffectNames
    {
        inline constexpr std::string_view ColourGrading = "colour_grading";
        inline constexpr std::string_view Vignette = "vignette";
        inline constexpr std::string_view ChromaticAberration = "chromatic_aberration";
    }

    /** @brief Fallback list matching `EngineBlendableEffectNames` (validation when no active registry). */
    inline constexpr std::array<std::string_view, 3> ENGINE_DEFAULT_BLENDABLE_EFFECT_NAMES = {{
        EngineBlendableEffectNames::ColourGrading,
        EngineBlendableEffectNames::Vignette,
        EngineBlendableEffectNames::ChromaticAberration,
    }};

    /** @brief ADL tag for Identity / Deserialise without a T instance. */
    template<typename T>
    struct EffectTag {};

    /**
     * @brief Engine-registered effect ids for fast lookup (no string table at runtime).
     */
    struct EngineEffectIds
    {
        BlendableEffectId ColourGrading = INVALID_BLENDABLE_EFFECT_ID;
        BlendableEffectId Vignette = INVALID_BLENDABLE_EFFECT_ID;
        BlendableEffectId ChromaticAberration = INVALID_BLENDABLE_EFFECT_ID;
    };

    /**
     * @brief Type-erased descriptor for a blendable effect type.
     */
    struct BlendableEffectDesc
    {
        std::string_view Name{};
        std::size_t Size = 0;
        std::size_t Align = 0;

        void (*CreateIdentity)(void* dst) = nullptr;
        void (*Destroy)(void* dst) = nullptr;
        void (*Blend)(void* dst, const void* src, float weight) = nullptr;
        void (*Deserialise)(void* dst, const nlohmann::json& json) = nullptr;
        void (*Serialise)(nlohmann::json& json, const void* src) = nullptr;
    };

    template<typename T>
    concept BlendableEffectPayload = std::is_nothrow_destructible_v<T> && requires(const T& a, const T& b, float w, nlohmann::json& jout, const nlohmann::json& jin) {
        { Identity(EffectTag<T>{}) } -> std::same_as<T>;
        { Lerp(a, b, w) } -> std::same_as<T>;
        { Deserialise(EffectTag<T>{}, jin) } -> std::same_as<T>;
        { Serialise(jout, a) } -> std::same_as<void>;
    };

    /**
     * @brief Open registry of blendable effect types. Register during engine/game init; call Seal before use.
     */
    class WAYFINDER_API BlendableEffectRegistry
    {
    public:
        BlendableEffectRegistry() = default;
        ~BlendableEffectRegistry() = default;

        BlendableEffectRegistry(const BlendableEffectRegistry&) = delete;
        BlendableEffectRegistry& operator=(const BlendableEffectRegistry&) = delete;
        BlendableEffectRegistry(BlendableEffectRegistry&&) = delete;
        BlendableEffectRegistry& operator=(BlendableEffectRegistry&&) = delete;

        template<BlendableEffectPayload T>
        BlendableEffectId Register(std::string_view name);

        void Seal();

        [[nodiscard]] bool IsSealed() const
        {
            return m_sealed;
        }

        [[nodiscard]] const BlendableEffectDesc* Find(BlendableEffectId id) const;

        [[nodiscard]] std::optional<BlendableEffectId> FindIdByName(std::string_view name) const;

        [[nodiscard]] std::size_t RegisteredCount() const
        {
            return m_descs.size();
        }

        /**
         * @brief Active registry for scene load / validation (set from EngineRuntime::Initialise).
         */
        static void SetActiveInstance(BlendableEffectRegistry* registry);
        [[nodiscard]] static BlendableEffectRegistry* GetActiveInstance();

    private:
        std::vector<BlendableEffectDesc> m_descs;
        std::vector<std::string> m_names; // Owns storage for Name string_view
        bool m_sealed = false;
    };

} // namespace Wayfinder

#include "BlendableEffectRegistry.inl"
