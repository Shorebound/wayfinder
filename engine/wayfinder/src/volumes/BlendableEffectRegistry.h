#pragma once

#include "volumes/BlendableEffectTraits.h"
#include "wayfinder_exports.h"

#include <concepts>
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
     * @brief Type-erased descriptor for a blendable effect type.
     */
    struct BlendableEffectDesc
    {
        std::string Name{};
        std::size_t Size = 0;
        std::size_t Align = 0;

        void (*CreateIdentity)(void* dst) = nullptr;
        void (*Destroy)(void* dst) = nullptr;
        void (*Blend)(void* dst, const void* src, float weight) = nullptr;
        void (*Deserialise)(void* dst, const nlohmann::json& json) = nullptr;
        void (*Serialise)(nlohmann::json& json, const void* src) = nullptr;
    };

    template<typename T>
    concept BlendableEffectPayload = std::is_trivially_copyable_v<T> && std::is_nothrow_destructible_v<T> && requires(const T& a, const T& b, float w, nlohmann::json& jout, const nlohmann::json& jin) {
        { BlendableEffectTraits<T>::Identity() } -> std::same_as<T>;
        { BlendableEffectTraits<T>::Lerp(a, b, w) } -> std::same_as<T>;
        { BlendableEffectTraits<T>::Deserialise(jin) } -> std::same_as<T>;
        BlendableEffectTraits<T>::Serialise(jout, a);
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
         * @brief Global instance for scene load / validation (set from EngineRuntime::Initialise).
         *
         * @todo Remove once ComponentRegistry's function-pointer-based Apply/Validate/Serialise
         *       chain supports a context parameter. All other consumers now use DI.
         */
        static void SetActiveInstance(BlendableEffectRegistry* registry);
        [[nodiscard]] static BlendableEffectRegistry* GetActiveInstance();

    private:
        std::vector<BlendableEffectDesc> m_descs;
        bool m_sealed = false;
    };

} // namespace Wayfinder

#include "BlendableEffectRegistry.inl"
