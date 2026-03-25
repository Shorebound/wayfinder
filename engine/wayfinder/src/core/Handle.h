#pragma once

#include <concepts>
#include <cstdint>
#include <format>
#include <functional>

namespace Wayfinder
{
    /**
     * @brief Generational handle — a type-safe, compact resource identifier.
     *
     * Each handle carries a 20-bit index and a 12-bit generation counter packed into
     * a single 32-bit value. The tag type parameter (`TTag`) provides compile-time
     * type safety: a `Handle<GPUShaderTag>` cannot be accidentally passed where a
     * `Handle<GPUBufferTag>` is expected.
     *
     * Handles are validated by the owning `ResourcePool`, which checks the generation
     * counter to detect use-after-free at runtime.
     *
     * @tparam TTag A tag type that distinguishes handle domains at compile time.
     */
    template<typename TTag>
    struct Handle
    {
        uint32_t Index : 20 = 0;
        uint32_t Generation : 12 = 0;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return Generation != 0;
        }
        constexpr explicit operator bool() const noexcept
        {
            return IsValid();
        }
        constexpr auto operator<=>(const Handle&) const = default;

        static constexpr Handle Invalid() noexcept
        {
            return {};
        }
    };

    // ── OpaqueHandle ────────────────────────────────────────────

    /**
     * @brief Concept for an OpaqueHandle tag type.
     *
     * Tags must declare their storage type and sentinel value:
     * @code
     *   struct PhysicsBodyTag
     *   {
     *       using ValueType = uint32_t;
     *       static constexpr ValueType INVALID = 0xFFFFFFFF;
     *   };
     * @endcode
     */
    template<typename TTag>
    concept OpaqueHandleTag = requires {
        typename TTag::ValueType;
        { TTag::INVALID } -> std::convertible_to<typename TTag::ValueType>;
    };

    /**
     * @brief Type-safe wrapper for opaque third-party handles.
     *
     * Unlike Handle<TTag>, which imposes its own index/generation bit layout,
     * OpaqueHandle stores a value exactly as the external library provides it.
     * The tag determines the storage type and invalid sentinel.
     *
     * @tparam TTag A tag satisfying OpaqueHandleTag.
     */
    template<OpaqueHandleTag TTag>
    struct OpaqueHandle
    {
        using ValueType = typename TTag::ValueType;
        static constexpr ValueType INVALID_VALUE = TTag::INVALID;

        ValueType Value = INVALID_VALUE;

        constexpr OpaqueHandle() = default;
        constexpr explicit OpaqueHandle(ValueType value) : Value(value) {}

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return Value != INVALID_VALUE;
        }
        constexpr explicit operator bool() const noexcept
        {
            return IsValid();
        }
        constexpr auto operator<=>(const OpaqueHandle&) const = default;
    };

} // namespace Wayfinder

template<typename TTag>
struct std::hash<Wayfinder::Handle<TTag>>
{
    size_t operator()(const Wayfinder::Handle<TTag>& h) const noexcept
    {
        return std::hash<uint32_t>{}((static_cast<uint32_t>(h.Index) << 12) | h.Generation);
    }
};

template<Wayfinder::OpaqueHandleTag TTag>
struct std::hash<Wayfinder::OpaqueHandle<TTag>>
{
    size_t operator()(const Wayfinder::OpaqueHandle<TTag>& h) const noexcept
    {
        return std::hash<typename TTag::ValueType>{}(h.Value);
    }
};

template<Wayfinder::OpaqueHandleTag TTag>
struct std::formatter<Wayfinder::OpaqueHandle<TTag>> : std::formatter<typename TTag::ValueType>
{
    auto format(const Wayfinder::OpaqueHandle<TTag>& h, auto& ctx) const
    {
        return std::formatter<typename TTag::ValueType>::format(h.Value, ctx);
    }
};
