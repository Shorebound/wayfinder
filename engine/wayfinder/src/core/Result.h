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

        explicit Error(std::string message)
            : m_message(std::move(message))
        {
        }

        explicit Error(const char* message)
            : m_message(message ? message : "")
        {
        }

        [[nodiscard]] const std::string& GetMessage() const noexcept { return m_message; }

        /// Implicit conversion to string_view for convenient logging.
        [[nodiscard]] operator std::string_view() const noexcept { return m_message; }

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
    template <typename T, typename E = Error>
    using Result = std::expected<T, E>;

    /**
     * @brief Construct an unexpected Error from a std::string message.
     * @param message  Human-readable description of the failure.
     * @return An `std::unexpected<Error>` suitable for returning from a
     *         function that yields `Result<T>`.
     */
    [[nodiscard]] inline std::unexpected<Error> MakeError(std::string message)
    {
        return std::unexpected<Error>(Error(std::move(message)));
    }

    /**
     * @brief Construct an unexpected Error from a C-string literal.
     * @param message  Human-readable description of the failure.
     *                 A null pointer is treated as an empty message.
     * @return An `std::unexpected<Error>` suitable for returning from a
     *         function that yields `Result<T>`.
     */
    [[nodiscard]] inline std::unexpected<Error> MakeError(const char* message)
    {
        return std::unexpected<Error>(Error(message));
    }

} // namespace Wayfinder
