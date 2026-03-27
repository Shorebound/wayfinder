#pragma once

#include "wayfinder_exports.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <nlohmann/json.hpp>

namespace Wayfinder
{
    /// Maximum size for type-erased effect payload in VolumeEffect (SBO).
    inline constexpr std::size_t VOLUME_EFFECT_PAYLOAD_CAPACITY = 96;

    /** @brief Opaque handle returned by VolumeEffectRegistry::Register. */
    using VolumeEffectId = uint32_t;

    inline constexpr VolumeEffectId INVALID_VOLUME_EFFECT_ID = static_cast<VolumeEffectId>(-1);

    /** @brief ADL tag for Identity / Deserialise without a T instance. */
    template<typename T>
    struct EffectTag {};

    /**
     * @brief Engine-registered effect ids for fast lookup (no string table at runtime).
     */
    struct EngineEffectIds
    {
        VolumeEffectId ColourGrading = INVALID_VOLUME_EFFECT_ID;
        VolumeEffectId Vignette = INVALID_VOLUME_EFFECT_ID;
        VolumeEffectId ChromaticAberration = INVALID_VOLUME_EFFECT_ID;
    };

    /**
     * @brief Type-erased descriptor for a blendable volume effect type.
     */
    struct VolumeEffectDesc
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
    concept BlendableEffect = std::is_nothrow_destructible_v<T> && requires(const T& a, const T& b, float w, nlohmann::json& jout, const nlohmann::json& jin) {
        { Identity(EffectTag<T>{}) } -> std::same_as<T>;
        { Lerp(a, b, w) } -> std::same_as<T>;
        { Deserialise(EffectTag<T>{}, jin) } -> std::same_as<T>;
        { Serialise(jout, a) } -> std::same_as<void>;
    };

    /**
     * @brief Open registry of blendable volume effect types. Register during engine/game init; call Seal before use.
     */
    class WAYFINDER_API VolumeEffectRegistry
    {
    public:
        VolumeEffectRegistry() = default;
        ~VolumeEffectRegistry() = default;

        VolumeEffectRegistry(const VolumeEffectRegistry&) = delete;
        VolumeEffectRegistry& operator=(const VolumeEffectRegistry&) = delete;
        VolumeEffectRegistry(VolumeEffectRegistry&&) = delete;
        VolumeEffectRegistry& operator=(VolumeEffectRegistry&&) = delete;

        template<BlendableEffect T>
        VolumeEffectId Register(std::string_view name);

        void Seal();

        [[nodiscard]] bool IsSealed() const
        {
            return m_sealed;
        }

        [[nodiscard]] const VolumeEffectDesc* Find(VolumeEffectId id) const;

        [[nodiscard]] std::optional<VolumeEffectId> FindIdByName(std::string_view name) const;

        [[nodiscard]] std::size_t RegisteredCount() const
        {
            return m_descs.size();
        }

        /**
         * @brief Active registry for scene load / validation (set from RenderContext::Initialise).
         */
        static void SetActiveInstance(VolumeEffectRegistry* registry);
        [[nodiscard]] static VolumeEffectRegistry* GetActiveInstance();

    private:
        std::vector<VolumeEffectDesc> m_descs;
        std::vector<std::string> m_names; // Owns storage for Name string_view
        bool m_sealed = false;
    };

} // namespace Wayfinder

#include "VolumeEffectRegistry.inl"
