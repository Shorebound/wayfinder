#pragma once

#include <cstdint>
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

} // namespace Wayfinder

template<typename TTag>
struct std::hash<Wayfinder::Handle<TTag>>
{
    size_t operator()(const Wayfinder::Handle<TTag>& h) const noexcept
    {
        return std::hash<uint32_t>{}((static_cast<uint32_t>(h.Index) << 12) | h.Generation);
    }
};
