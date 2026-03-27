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
    /// Maximum size for type-erased effect payload in PostProcessEffect (SBO).
    inline constexpr std::size_t POST_PROCESS_EFFECT_PAYLOAD_CAPACITY = 96;

    /** @brief Opaque handle returned by PostProcessRegistry::Register. */
    using PostProcessEffectId = uint32_t;

    inline constexpr PostProcessEffectId INVALID_POST_PROCESS_EFFECT_ID = static_cast<PostProcessEffectId>(-1);

    /** @brief ADL tag for Identity / Deserialise without a T instance. */
    template<typename T>
    struct PostProcessTag {};

    /**
     * @brief Engine-registered effect ids for fast lookup (no string table at runtime).
     */
    struct EnginePostProcessIds
    {
        PostProcessEffectId ColourGrading = INVALID_POST_PROCESS_EFFECT_ID;
        PostProcessEffectId Vignette = INVALID_POST_PROCESS_EFFECT_ID;
        PostProcessEffectId ChromaticAberration = INVALID_POST_PROCESS_EFFECT_ID;
    };

    /**
     * @brief Type-erased descriptor for a blendable post-process effect type.
     */
    struct PostProcessEffectDesc
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
    concept BlendablePostProcessEffect = std::is_nothrow_destructible_v<T> && requires(const T& a, const T& b, float w, nlohmann::json& jout, const nlohmann::json& jin) {
        { Identity(PostProcessTag<T>{}) } -> std::same_as<T>;
        { Lerp(a, b, w) } -> std::same_as<T>;
        { Deserialise(PostProcessTag<T>{}, jin) } -> std::same_as<T>;
        { Serialise(jout, a) } -> std::same_as<void>;
    };

    /**
     * @brief Open registry of blendable post-process effect types. Register during engine/game init; call Seal before use.
     */
    class WAYFINDER_API PostProcessRegistry
    {
    public:
        PostProcessRegistry() = default;
        ~PostProcessRegistry() = default;

        PostProcessRegistry(const PostProcessRegistry&) = delete;
        PostProcessRegistry& operator=(const PostProcessRegistry&) = delete;
        PostProcessRegistry(PostProcessRegistry&&) = delete;
        PostProcessRegistry& operator=(PostProcessRegistry&&) = delete;

        template<BlendablePostProcessEffect T>
        PostProcessEffectId Register(std::string_view name);

        void Seal();

        [[nodiscard]] bool IsSealed() const
        {
            return m_sealed;
        }

        [[nodiscard]] const PostProcessEffectDesc* Find(PostProcessEffectId id) const;

        [[nodiscard]] std::optional<PostProcessEffectId> FindIdByName(std::string_view name) const;

        [[nodiscard]] std::size_t RegisteredCount() const
        {
            return m_descs.size();
        }

        /**
         * @brief Active registry for scene load / validation (set from RenderContext::Initialise).
         */
        static void SetActiveInstance(PostProcessRegistry* registry);
        [[nodiscard]] static PostProcessRegistry* GetActiveInstance();

    private:
        std::vector<PostProcessEffectDesc> m_descs;
        std::vector<std::string> m_names; // Owns storage for Name string_view
        bool m_sealed = false;
    };

} // namespace Wayfinder

#include "PostProcessRegistry.inl"
