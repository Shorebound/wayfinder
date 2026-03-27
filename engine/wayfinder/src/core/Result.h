#pragma once

#include "wayfinder_exports.h"

#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace Wayfinder
{

    /**
     * @brief Lightweight, value-semantic error type carried in Result.
     *
     * Wraps a human-readable message string.  Deliberately simple:
     * the engine logs context at the point of failure, so the Error
     * only needs to carry enough information for the immediate caller
     * to decide what to do.
     */
    class Error
    {
    public:
        Error() = default;

        explicit Error(std::string_view message) : m_message(message) {}

        [[nodiscard]] const std::string& GetMessage() const noexcept
        {
            return m_message;
        }

        /// Implicit conversion to string_view for convenient logging.
        [[nodiscard]] operator std::string_view() const noexcept
        {
            return m_message;
        }

        bool operator==(const Error& other) const = default;

    private:
        std::string m_message;
    };

    /**
     * @brief Engine-wide return type for operations that can fail.
     *
     * Thin alias over `std::expected<T, E>` with a default error type
     * of Wayfinder::Error.
     *
     * @tparam T  The value type on success (may be `void`).
     * @tparam E  The error type on failure (defaults to Wayfinder::Error).
     */
    template<typename T, typename E = Error>
    using Result = std::expected<T, E>;

    /**
     * @brief Construct an unexpected Error from a message.
     * @param message  Human-readable description of the failure.
     * @return An `std::unexpected<Error>` suitable for returning from a
     *         function that yields `Result<T>`.
     */
    /**
     * @brief Construct an unexpected Error from a message (literals, `std::string`, or `std::format` output).
     */
    [[nodiscard]] inline std::unexpected<Error> MakeError(std::string_view message)
    {
        return std::unexpected<Error>(Error(message));
    }

} // namespace Wayfinder
