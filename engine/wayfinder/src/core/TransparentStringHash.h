#pragma once

#include <cstddef>
#include <functional>
#include <string_view>

namespace Wayfinder
{
    /**
     * @brief Hash functor for `std::unordered_map` / `std::unordered_set` keyed by `std::string` with heterogeneous lookup.
     *
     * Pair with `std::equal_to<>` as the key equality predicate so `find`, `contains`, and `erase` accept `std::string_view`
     * without constructing a temporary `std::string` when the key already exists. MSVC requires `is_transparent` on the
     * hasher (not only `std::hash<std::string_view>` as the template parameter).
     */
    struct TransparentStringHash
    {
        // NOLINTNEXTLINE(readability-identifier-naming) — required for transparent container lookup
        using is_transparent = void;

        std::size_t operator()(std::string_view key) const noexcept
        {
            return std::hash<std::string_view>{}(key);
        }
    };

} // namespace Wayfinder
