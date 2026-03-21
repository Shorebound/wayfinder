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
            : m_message(message)
        {
        }

        [[nodiscard]] const std::string& GetMessage() const noexcept { return m_message; }

        /// Implicit conversion to string_view for convenient logging.
        [[nodiscard]] operator std::string_view() const noexcept { return m_message; }

        bool operator==(const Error& other) const = default;

    private:
        std::string m_message;
    };

    // ── Result alias ─────────────────────────────────────────
    //
    // Result<T, E> is the engine-wide return type for operations that
    // can fail.  It is a thin alias over std::expected<T, E> with a
    // default error type of Wayfinder::Error.
    //
    //   Result<int>          — returns int  on success, Error on failure
    //   Result<void>         — returns void on success, Error on failure
    //   Result<Foo, BarErr>  — custom error type override
    //

    template <typename T, typename E = Error>
    using Result = std::expected<T, E>;

    // ── Convenience factory ──────────────────────────────────

    /// Construct an unexpected Error from a message string.
    [[nodiscard]] inline std::unexpected<Error> MakeError(std::string message)
    {
        return std::unexpected<Error>(Error(std::move(message)));
    }

    /// Construct an unexpected Error from a C-string literal.
    [[nodiscard]] inline std::unexpected<Error> MakeError(const char* message)
    {
        return std::unexpected<Error>(Error(message));
    }

} // namespace Wayfinder
